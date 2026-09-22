from __future__ import annotations

import json
import queue
import shlex
import threading
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Tuple

import frida
from prompt_toolkit.application import Application
from prompt_toolkit.application.current import get_app
from prompt_toolkit.data_structures import Point
from prompt_toolkit.filters import Condition, has_focus
from prompt_toolkit.formatted_text import FormattedText
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout import Layout
from prompt_toolkit.layout.containers import (
    ConditionalContainer,
    DynamicContainer,
    Float,
    FloatContainer,
    HSplit,
    VSplit,
    Window,
)
from prompt_toolkit.layout.controls import FormattedTextControl, UIContent, UIControl
from prompt_toolkit.layout.dimension import Dimension
from prompt_toolkit.widgets import Frame, TextArea

from frida_tools.application import ConsoleApplication
from frida_tools.reactor import Reactor


def main() -> None:
    app = StraceApplication()
    app.run()


class StraceApplication(ConsoleApplication):
    def __init__(self) -> None:
        self._state = "starting"
        self._ready = threading.Event()

        super().__init__(self._process_input)

        self._tracer: Optional[SyscallTracer] = None

        self._events: List[SyscallEvent] = []
        self._excluded_syscalls: set[tuple[str, int]] = set()
        self._selected = 0
        self._hscroll = 0
        self._tailing = True

        self._paused_events: Optional[List[SyscallEvent]] = None
        self._pending_by_key: Dict[Tuple[int, int, int], int] = {}

        self._show_details = True
        self._show_help = False
        self._details_breakpoint_cols = 120

        self._lock = threading.RLock()

        self._ui_app: Optional[Application] = None
        self._list_win: Optional[Window] = None

        self._filter_text = ""

        self._search_text = ""
        self._search_editing = False
        self._search_prev_text = ""
        self._search_matches: List[int] = []
        self._search_index = 0

        self._ignore_search_text_change = False
        self._status_message = ""

        self._fd_style_by_pid_value: Dict[Tuple[int, int], str] = {}
        self._fd_color_palette = [
            "ansiblue",
            "ansigreen",
            "ansicyan",
            "ansimagenta",
            "ansiyellow",
            "ansibrightblue",
            "ansibrightgreen",
            "ansibrightcyan",
            "ansibrightmagenta",
            "ansibrightyellow",
        ]
        self._next_fd_color = 0

        self._list_model_version = 0
        self._filter_cache_key: Optional[Tuple[int, str, bool, int]] = None
        self._filter_cache: List[SyscallEvent] = []

        self._list_model = ListModel(
            snapshot=self._get_list_snapshot,
            format_dt=self._format_dt,
            format_line=self._format_event_line,
            highlight_line=self._highlight_line,
        )
        self._list_ctl: UIControl = SyscallListControl(self._list_model)

        self._detail_ctl = FormattedTextControl(text=self._get_detail_text)
        self._status_ctl = FormattedTextControl(text=self._get_status_text)

        self._search_bar = TextArea(
            height=1,
            prompt="/",
            multiline=False,
            wrap_lines=False,
        )
        self._search_bar.buffer.on_text_changed += self._on_search_text_changed

        self._platform: Optional[str] = None
        self._ui_refresh_pending = False

    def _needs_target(self) -> bool:
        return False

    def _add_options(self, parser) -> None:
        parser.add_argument(
            "-f",
            "--file",
            action="append",
            dest="files",
            help="spawn FILE [args...] (repeatable); pass as one string, e.g. -f '/bin/ls -l /'",
        )
        parser.add_argument("-p", "--pid", action="append", type=int, dest="pids", help="Trace PID (repeatable)")
        parser.add_argument(
            "-u", "--user", action="append", dest="users", help="Trace processes owned by USER (repeatable)"
        )
        parser.add_argument("--uid", action="append", type=int, dest="uids", help="Trace UID (repeatable)")
        parser.add_argument(
            "-x",
            "--exclude",
            action="append",
            dest="exclude",
            help="Exclude syscall by name (repeatable), e.g. --exclude openat",
        )
        parser.add_argument(
            "-i",
            "--include",
            action="append",
            dest="include",
            help="Only trace syscall by name (repeatable), e.g. --include openat",
        )
        parser.add_argument("--limit", type=int, default=5000, help="Max events kept in UI (default: 5000)")

    def _initialize(self, parser, options, args) -> None:
        self._files = options.files
        self._pids = options.pids
        self._users = options.users
        self._uids = options.uids
        self._excluded_names_requested = options.exclude
        self._included_names_requested = options.include
        self._limit = options.limit

        if self._files is None and self._pids is None and self._users is None and self._uids is None:
            raise ValueError("At least one target must be specified (use --file, --pid, --user, and/or --uid).")

        if self._excluded_names_requested and self._included_names_requested:
            raise ValueError("--exclude and --include are mutually exclusive.")

    def _usage(self) -> str:
        return "%(prog)s [options]"

    def _start(self) -> None:
        params = self._device.query_system_parameters()
        self._platform = params["platform"]

        self._tracer = SyscallTracer(self._reactor)
        self._tracer.set_notify(self._notify_ui)
        self._tracer.set_platform(self._platform)

        spawned_pids: List[int] = []
        pids: List[int] = []
        if self._files is not None:
            for spec in self._files:
                argv = shlex.split(spec)
                if not argv:
                    continue
                program = argv[0]
                pid = self._device.spawn(program, argv if len(argv) > 1 else None)
                spawned_pids.append(pid)
                pids.append(pid)
        if self._pids is not None:
            pids.extend(self._pids)

        targets_req: Dict[str, Any] = {}
        if pids:
            targets_req["pids"] = pids
        if self._users is not None:
            targets_req["users"] = self._users
        if self._uids is not None:
            targets_req["uids"] = self._uids

        try:
            self._tracer.start(
                self._device, targets_req, self._excluded_names_requested, self._included_names_requested
            )
        except Exception as e:
            self._state = "stopping"
            self._ready.set()
            self._log("error", f"Unable to start: {e}")
            self._exit(1)
            return
        finally:
            for pid in spawned_pids:
                self._device.resume(pid)

        excluded = self._tracer.get_excluded()
        if excluded:
            with self._lock:
                self._excluded_syscalls = set(excluded)

        self._ui_app = self._create_ui()

        self._state = "started"
        self._ready.set()

    def _stop(self) -> None:
        if self._tracer is not None:
            try:
                self._tracer.stop()
            except Exception:
                pass
            self._tracer = None

        if self._ui_app is not None:
            try:
                self._ui_app.exit()
            except Exception:
                pass
            self._ui_app = None

    def _process_input(self, reactor: Reactor) -> None:
        try:
            while not self._ready.wait(0.5):
                if not reactor.is_running():
                    return
        except KeyboardInterrupt:
            reactor.cancel_io()
            return

        if self._state != "started":
            return

        assert self._ui_app is not None
        self._ui_app.run()

    def _create_ui(self) -> Application:
        kb = KeyBindings()

        @kb.add("q")
        @kb.add("c-c")
        def _(event):
            self._reactor.cancel_io()
            self._exit(0)
            event.app.exit()

        @kb.add("/", filter=~has_focus(self._search_bar.control))
        def _(event):
            self._begin_search(event)

        @kb.add("down", filter=~has_focus(self._search_bar.control))
        @kb.add("j", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._move_selected_locked(+1)
            event.app.invalidate()

        @kb.add("up", filter=~has_focus(self._search_bar.control))
        @kb.add("k", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._move_selected_locked(-1)
            event.app.invalidate()

        @kb.add("c-b", filter=~has_focus(self._search_bar.control))
        @kb.add("pageup", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                delta = self._page_delta_locked(-1, fraction=1.0)
                self._move_selected_locked(delta)
            event.app.invalidate()

        @kb.add("c-f", filter=~has_focus(self._search_bar.control))
        @kb.add("pagedown", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                delta = self._page_delta_locked(+1, fraction=1.0)
                self._move_selected_locked(delta)
            event.app.invalidate()

        @kb.add("c-d", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                delta = self._page_delta_locked(+1, fraction=0.5)
                self._move_selected_locked(delta)
            event.app.invalidate()

        @kb.add("c-u", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                delta = self._page_delta_locked(-1, fraction=0.5)
                self._move_selected_locked(delta)
            event.app.invalidate()

        @kb.add("home", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._pause_if_tailing()
                self._selected = 0
                self._clamp_selected_locked()
            event.app.invalidate()

        @kb.add("t", filter=~has_focus(self._search_bar.control))
        @kb.add("end", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._tailing = True
                self._paused_events = None
                self._bump_list_model_version_locked()
                self._invalidate_filter_cache_locked()
                self._recompute_search_matches_locked()
                view = self._get_filtered_view_locked()
                self._selected = max(0, len(view) - 1)
            assert self._list_win is not None
            event.app.layout.focus(self._list_win)
            event.app.invalidate()

        @kb.add("left", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._hscroll = max(0, self._hscroll - 8)
            event.app.invalidate()

        @kb.add("right", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._hscroll += 8
            event.app.invalidate()

        @kb.add("0", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._hscroll = 0
            event.app.invalidate()

        @kb.add("enter", filter=has_focus(self._search_bar.control))
        def _(event):
            self._end_search(keep=True, event=event)

        @kb.add("escape", filter=has_focus(self._search_bar.control))
        def _(event):
            self._end_search(keep=False, event=event)

        @kb.add("c-u", filter=has_focus(self._search_bar.control))
        def _(event):
            self._set_search_bar_text("")
            event.app.invalidate()

        @kb.add("n", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._pause_if_tailing()
                self._recompute_search_matches_locked()
                if self._search_matches:
                    self._status_message = ""
                    self._search_index = (self._search_index + 1) % len(self._search_matches)
                    self._selected = self._search_matches[self._search_index]
            event.app.invalidate()

        @kb.add("N", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._pause_if_tailing()
                self._recompute_search_matches_locked()
                if self._search_matches:
                    self._status_message = ""
                    self._search_index = (self._search_index - 1) % len(self._search_matches)
                    self._selected = self._search_matches[self._search_index]
            event.app.invalidate()

        @kb.add("f", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                if self._search_text != "":
                    self._filter_text = self._search_text
                    self._search_text = ""
                    self._search_prev_text = ""
                    self._search_matches = []
                    self._search_index = 0

                    self._tailing = True
                    self._paused_events = None

                    self._bump_list_model_version_locked()
                    self._invalidate_filter_cache_locked()
                    view = self._get_filtered_view_locked()
                    self._selected = max(0, len(view) - 1)
            event.app.invalidate()

        @kb.add("c-l", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._filter_text = ""
                self._invalidate_filter_cache_locked()
                self._bump_list_model_version_locked()
                self._recompute_search_matches_locked()
                self._clamp_selected_locked()
            event.app.invalidate()

        @kb.add("d", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                self._show_details = not self._show_details
            event.app.layout.focus(self._list_win)
            event.app.invalidate()

        @kb.add("enter", filter=~has_focus(self._search_bar.control))
        def _(event):
            self._on_enter()
            event.app.invalidate()

        @kb.add("x", filter=~has_focus(self._search_bar.control))
        def _(event):
            with self._lock:
                view = self._get_filtered_view_locked()
                if not (0 <= self._selected < len(view)):
                    return
                ev = view[self._selected]

                key = (ev.abi, ev.nr)
                if key in self._excluded_syscalls:
                    self._status_message = f"Already excluded: {ev.name} ({ev.abi} #{ev.nr})"
                    event.app.invalidate()
                    return

                self._excluded_syscalls.add(key)
                removed = self._exclude_events_locked(key)

                self._status_message = f"Excluded: {ev.name} ({ev.abi} #{ev.nr})"
                if removed:
                    self._status_message += f" (purged {removed} events)"

            if ev.nr >= 0:
                self._tracer.exclude_syscalls([key])

            event.app.invalidate()

        @kb.add("?", filter=~has_focus(self._search_bar.control))
        def _(event):
            self._show_help = not self._show_help
            event.app.invalidate()

        @kb.add("escape", filter=~has_focus(self._search_bar.control))
        def _(event):
            if self._show_help:
                self._show_help = False
                event.app.invalidate()

        @kb.add("w", filter=~has_focus(self._search_bar.control))
        def _(event):
            self._export_events()
            event.app.invalidate()

        list_win = Window(
            content=self._list_ctl,
            wrap_lines=False,
            cursorline=True,
            always_hide_cursor=False,
            get_horizontal_scroll=lambda w: self._hscroll,
        )
        self._list_win = list_win

        detail_win = Window(content=self._detail_ctl, wrap_lines=True, always_hide_cursor=True)
        status_win = Window(height=1, content=self._status_ctl, wrap_lines=False)

        list_frame = Frame(list_win, title="Syscalls")
        detail_frame = Frame(detail_win, title="Details")
        status_frame = Frame(status_win, title="Status")

        def panes_container():
            if not self._show_details:
                return list_frame

            cols = get_app().output.get_size().columns
            if cols < self._details_breakpoint_cols:
                return HSplit([list_frame, detail_frame])

            return VSplit(
                [
                    Frame(list_win, title="Syscalls", width=Dimension(weight=3)),
                    Frame(detail_win, title="Details", width=Dimension(weight=2)),
                ]
            )

        body = HSplit(
            [
                DynamicContainer(panes_container),
                status_frame,
            ]
        )

        search_win = Window(height=1, content=self._search_bar.control, wrap_lines=False)
        search_float = ConditionalContainer(
            content=Frame(search_win, title="Search"),
            filter=Condition(lambda: self._search_editing),
        )

        help_text = "\n".join(
            [
                "",
                "  Navigation",
                "    j/k ↑/↓         move selection",
                "    PgUp/PgDn       page up/down",
                "    ^D/^U           half-page down/up",
                "    Home  End  t    top / tail",
                "    ←/→  0          scroll / reset",
                "",
                "  Search & Filter",
                "    /               search",
                "    n/N             next/prev match",
                "    f               promote search to filter",
                "    ^L              clear filter",
                "",
                "  Actions",
                "    Enter           resolve selected stack",
                "    x               exclude syscall",
                "    w               export view to JSON",
                "    d               toggle detail pane",
                "",
                "  ?  close this help    q/^C  quit",
                "",
            ]
        )
        help_win = Window(
            content=FormattedTextControl(text=help_text),
            wrap_lines=False,
            always_hide_cursor=True,
        )
        help_float = ConditionalContainer(
            content=Frame(help_win, title="Help"),
            filter=Condition(lambda: self._show_help),
        )

        root = FloatContainer(
            content=body,
            floats=[
                Float(
                    content=search_float,
                    bottom=1,
                    left=0,
                    right=0,
                    height=3,
                ),
                Float(
                    content=help_float,
                    xcursor=False,
                    ycursor=False,
                ),
            ],
        )

        layout = Layout(root, focused_element=list_win)
        app = Application(layout=layout, key_bindings=kb, full_screen=True)
        return app

    def _get_list_snapshot(self) -> ListSnapshot:
        with self._lock:
            base = self._get_base_view_locked()
            view = self._get_filtered_view_locked()
            cache_key = (
                self._list_model_version,
                self._filter_text,
                self._tailing,
                id(base),
                self._search_text,
                self._search_editing,
            )
            return ListSnapshot(
                cache_key=cache_key,
                view=view,
                selected=self._selected,
                search_text=self._search_text,
            )

    def _format_event_line(self, ev: SyscallEvent, dt: str) -> FormattedText:
        phase = "→" if ev.phase == "enter" else "←"
        if ev.merged:
            phase = "↔"

        line_style = "fg:ansired" if ev.failed else ""

        line: FormattedText = []
        line.append(("", f"{dt} "))
        line.append((line_style, f"[{ev.pid}:{ev.tid}] {phase} {ev.name}("))

        if ev.enter_args is not None:
            line.extend(self._format_args(ev.pid, ev.enter_args))
        else:
            line.append((line_style, ev.enter_summary or ""))

        line.append((line_style, ")"))

        if ev.exit_retval is not None:
            line.append((line_style, " => "))
            line.extend(self._format_retval(ev, ev.exit_retval, failed=ev.failed))

            if ev.exit_out_args is not None:
                line.append((line_style, ", "))
                line.extend(self._format_args(ev.pid, ev.exit_out_args))

        return line

    def _begin_search(self, event) -> None:
        with self._lock:
            self._pause_if_tailing()
            self._status_message = ""
            self._search_prev_text = self._search_text
            self._search_text = ""
            self._search_editing = True
            self._recompute_search_matches_locked()
            if self._search_matches:
                self._search_index = 0
                self._selected = self._search_matches[0]
            else:
                view = self._get_filtered_view_locked()
                self._selected = 0 if view else 0

        self._set_search_bar_text("")
        event.app.layout.focus(self._search_bar.control)
        event.app.invalidate()

    def _end_search(self, keep: bool, event) -> None:
        with self._lock:
            if not keep:
                self._search_text = self._search_prev_text

            self._search_editing = False
            self._recompute_search_matches_locked()

            view = self._get_filtered_view_locked()

            if keep:
                if self._search_text != "" and not self._search_matches:
                    self._status_message = "Pattern not found"
                    self._selected = 0 if view else 0
                elif self._search_matches:
                    self._status_message = ""
                    self._search_index = 0
                    self._selected = self._search_matches[0]

        if not keep:
            self._set_search_bar_text(self._search_prev_text)

        assert self._list_win is not None
        event.app.layout.focus(self._list_win)
        event.app.invalidate()

    def _set_search_bar_text(self, text: str) -> None:
        self._ignore_search_text_change = True
        try:
            self._search_bar.text = text
        finally:
            self._ignore_search_text_change = False

    def _on_search_text_changed(self, _) -> None:
        if self._ui_app is None or self._ignore_search_text_change:
            return

        with self._lock:
            if not self._search_editing:
                return
            self._search_text = self._search_bar.text
            self._recompute_search_matches_locked()

            if self._search_matches:
                self._status_message = ""
                self._search_index = 0
                self._selected = self._search_matches[0]

        self._ui_app.invalidate()

    def _recompute_search_matches_locked(self) -> None:
        f = self._search_text
        if f == "":
            self._search_matches = []
            self._search_index = 0
            return

        f_lc = f.lower()
        view = self._get_filtered_view_locked()
        matches: List[int] = []
        for i, ev in enumerate(view):
            if self._event_matches_filter(ev, f_lc):
                matches.append(i)
        self._search_matches = matches
        if self._search_index >= len(matches):
            self._search_index = 0

    def _move_selected_locked(self, delta: int) -> None:
        self._pause_if_tailing()
        self._selected += delta
        self._clamp_selected_locked()

    def _clamp_selected_locked(self) -> None:
        view = self._get_filtered_view_locked()
        if not view:
            self._selected = 0
        else:
            self._selected = max(0, min(self._selected, len(view) - 1))

    def _page_delta_locked(self, direction: int, fraction: float = 1.0) -> int:
        self._pause_if_tailing()
        assert self._list_win is not None
        height = self._list_win.render_info.window_height
        page = max(1, height - 1)
        step = max(1, int(page * fraction))
        return direction * step

    def _pause_if_tailing(self) -> None:
        if self._tailing:
            self._tailing = False
            self._paused_events = list(self._events)
            self._selected = min(self._selected, max(0, len(self._paused_events) - 1)) if self._paused_events else 0
            self._bump_list_model_version_locked()
            self._invalidate_filter_cache_locked()
            self._recompute_search_matches_locked()

    def _get_base_view_locked(self) -> List[SyscallEvent]:
        return self._events if self._tailing else self._paused_events

    def _invalidate_filter_cache_locked(self) -> None:
        self._filter_cache_key = None
        self._filter_cache = []

    def _get_filtered_view_locked(self) -> List[SyscallEvent]:
        base = self._get_base_view_locked()
        key = (self._list_model_version, self._filter_text, self._tailing, id(base))
        if self._filter_cache_key == key:
            return self._filter_cache

        f = self._filter_text
        if f == "":
            self._filter_cache = base
            self._filter_cache_key = key
            return self._filter_cache

        f_lc = f.lower()
        out: List[SyscallEvent] = []
        for ev in base:
            if self._event_matches_filter(ev, f_lc):
                out.append(ev)

        self._filter_cache = out
        self._filter_cache_key = key
        return self._filter_cache

    def _event_matches_filter(self, ev: SyscallEvent, f_lc: str) -> bool:
        if f_lc in ev.name.lower():
            return True
        if f_lc in str(ev.pid) or f_lc in str(ev.tid) or f_lc in str(ev.nr):
            return True
        if ev.enter_args is not None:
            for a in ev.enter_args:
                if f_lc in a.name.lower():
                    return True
                if f_lc in a.text.lower():
                    return True
        if ev.exit_retval is not None and f_lc in str(ev.exit_retval).lower():
            return True
        return False

    def _notify_ui(self) -> None:
        if self._ui_app is None:
            return
        with self._lock:
            if self._ui_refresh_pending:
                return
            self._ui_refresh_pending = True
        self._ui_app.loop.call_soon_threadsafe(self._drain_and_refresh_on_ui)

    def _drain_and_refresh_on_ui(self) -> None:
        tracer = self._tracer
        if tracer is None:
            with self._lock:
                self._ui_refresh_pending = False
            return

        changed_list = False

        while True:
            new_events = tracer.drain_events(limit=5000)
            updates = tracer.drain_updates(limit=5000)

            if not new_events and not updates:
                break

            with self._lock:
                if new_events:
                    for ev in new_events:
                        changed_list |= self._append_or_merge_event_locked(ev)

                    if len(self._events) > self._limit:
                        overflow = len(self._events) - self._limit

                        if overflow > 0:
                            new_pending: Dict[Tuple[int, int, int], int] = {}
                            for k, idx in self._pending_by_key.items():
                                if idx >= overflow:
                                    new_pending[k] = idx - overflow
                            self._pending_by_key = new_pending

                        self._events = self._events[overflow:]
                        changed_list = True

                        if self._tailing and not self._search_editing:
                            self._selected = max(0, self._selected - overflow)

                    if self._tailing and not self._search_editing:
                        view = self._get_filtered_view_locked()
                        self._selected = max(0, len(view) - 1)

                if updates:
                    id_to_event = {e.id: e for e in self._events}
                    if self._paused_events is not None:
                        for e in self._paused_events:
                            id_to_event.setdefault(e.id, e)

                    for event_id, stack_or_none, syms_or_exc in updates:
                        ev = id_to_event.get(event_id)
                        if ev is None:
                            continue
                        ev.resolving = False
                        if stack_or_none is None:
                            ev.resolve_error = str(syms_or_exc)
                        else:
                            ev.stack = stack_or_none
                            ev.symbols = syms_or_exc
                            ev.resolve_error = None

                if changed_list:
                    self._bump_list_model_version_locked()
                    self._invalidate_filter_cache_locked()
                    self._recompute_search_matches_locked()

        with self._lock:
            self._ui_refresh_pending = False

        assert self._ui_app is not None
        self._ui_app.invalidate()

    def _bump_list_model_version_locked(self) -> None:
        self._list_model_version += 1

    def _append_or_merge_event_locked(self, ev: SyscallEvent) -> bool:
        if ev.phase == "exit":
            key = (ev.pid, ev.tid, ev.nr)
            idx = self._pending_by_key.get(key)
            if idx is not None and 0 <= idx < len(self._events):
                prev = self._events[idx]
                if prev.phase == "enter" and prev.nr == ev.nr and prev.pid == ev.pid and prev.tid == ev.tid:
                    prev.set_exit(ev.exit_retval, ev.exit_out_args, ev.time_ns, ev.stack_id)
                    return True

        self._events.append(ev)
        if ev.phase == "enter":
            self._pending_by_key[(ev.pid, ev.tid, ev.nr)] = len(self._events) - 1
        return True

    def _on_enter(self) -> None:
        tracer = self._tracer
        if tracer is None:
            return

        with self._lock:
            view = self._get_filtered_view_locked()
            if not (0 <= self._selected < len(view)):
                return
            ev = view[self._selected]
            if ev.stack_id < 0 or ev.resolving or ev.stack is not None:
                return
            ev.resolving = True

        tracer.resolve_backtrace(ev.id, ev.pid, ev.map_gen, ev.stack_id)

    def _export_events(self) -> None:
        with self._lock:
            view = self._get_filtered_view_locked()
            rows = [self._serialize_event(ev) for ev in view]

        filename = f"stracer-export-{datetime.now().strftime('%Y%m%d-%H%M%S')}.json"
        with open(filename, "w") as f:
            json.dump(rows, f, indent=2, default=str)

        with self._lock:
            self._status_message = f"Exported {len(rows)} events → {filename}"

    def _serialize_event(self, ev: SyscallEvent) -> dict:
        d: dict = {
            "id": ev.id,
            "phase": ev.phase,
            "pid": ev.pid,
            "tid": ev.tid,
            "abi": ev.abi,
            "nr": ev.nr,
            "name": ev.name,
            "time_ns": ev.time_ns,
            "merged": ev.merged,
            "failed": ev.failed,
        }

        if ev.enter_args is not None:
            d["enter_args"] = [{"name": a.name, "type": a.type, "text": a.text} for a in ev.enter_args]
        if ev.exit_retval is not None:
            d["exit_retval"] = ev.exit_retval
            d["exit_time_ns"] = ev.exit_time_ns
        if ev.exit_out_args is not None:
            d["exit_out_args"] = [{"name": a.name, "type": a.type, "text": a.text} for a in ev.exit_out_args]

        if ev.stack is not None:
            d["stack"] = [f"0x{addr:x}" for addr in ev.stack]
            d["call_stack"] = self._format_call_stack(ev)

        return d

    def _exclude_events_locked(self, key: tuple[str, int]) -> None:
        abi, nr = key

        def keep(e: SyscallEvent) -> bool:
            return not (e.abi == abi and e.nr == nr)

        before = len(self._events)
        self._events = [e for e in self._events if keep(e)]
        removed = before - len(self._events)

        if self._paused_events is not None:
            self._paused_events = [e for e in self._paused_events if keep(e)]

        self._pending_by_key.clear()
        for i, e in enumerate(self._events):
            if e.phase == "enter":
                self._pending_by_key[(e.pid, e.tid, e.nr)] = i

        self._bump_list_model_version_locked()
        self._invalidate_filter_cache_locked()
        self._recompute_search_matches_locked()
        self._clamp_selected_locked()

        return removed

    def _highlight_line(self, ft: FormattedText, needle_lc: str) -> FormattedText:
        plain = "".join(text for _, text in ft)
        hay_lc = plain.lower()
        pos = hay_lc.find(needle_lc)
        if pos < 0:
            return ft

        end = pos + len(needle_lc)

        out: FormattedText = []
        i = 0
        for style, text in ft:
            if text == "":
                continue

            chunk_start = i
            chunk_end = i + len(text)

            if chunk_end <= pos or chunk_start >= end:
                out.append((style, text))
            else:
                a = max(0, pos - chunk_start)
                b = min(len(text), end - chunk_start)

                if a > 0:
                    out.append((style, text[:a]))
                if b > a:
                    out.append(("reverse bold", text[a:b]))
                if b < len(text):
                    out.append((style, text[b:]))

            i = chunk_end

        return out

    def _format_args(self, pid: int, args: List[Arg]) -> FormattedText:
        out: FormattedText = []
        for j, a in enumerate(args):
            if j != 0:
                out.append(("", ", "))

            out.append(("", f"{a.name}="))

            if a.kind == "string":
                out.append(("fg:ansigreen", a.text))
            elif a.kind == "bytes":
                out.append(("fg:ansiyellow", a.text))
            elif a.is_fd:
                out.append((self._fd_style(pid, a.value), a.text))
            else:
                out.append(("", a.text))
        return out

    def _format_retval(self, ev: "SyscallEvent", v: Any, failed: bool) -> FormattedText:
        if self._platform == "linux":
            if failed:
                eno = -v
                name = LINUX_ERRNO.get(eno)
                if name is not None:
                    return FormattedText([("fg:ansired bold", f"-{eno} {name}")])
                return FormattedText([("fg:ansired bold", f"-{eno} errno={eno}")])

            if ev.name in ("mmap", "mremap"):
                if v == 0:
                    return FormattedText([("fg:ansigreen", "NULL")])
                return FormattedText([("", f"0x{v:x}")])

        if failed:
            return FormattedText([("fg:ansired bold", str(v))])

        if self._syscall_returns_fd(ev.name):
            fd = to_signed32(v)
            return FormattedText([(self._fd_style(ev.pid, fd), str(fd))])

        return FormattedText([("", str(v))])

    def _syscall_returns_fd(self, name: str) -> bool:
        return self._platform == "linux" and name in LINUX_FD_RETURNING_SYSCALLS

    def _fd_style(self, pid: int, fd: int) -> str:
        key = (pid, fd)
        style = self._fd_style_by_pid_value.get(key)
        if style is None:
            color = self._fd_color_palette[self._next_fd_color % len(self._fd_color_palette)]
            self._next_fd_color += 1
            style = f"fg:{color} bold"
            self._fd_style_by_pid_value[key] = style
        return style

    def _format_dt(self, delta_ns: int) -> str:
        if delta_ns < 0:
            delta_ns = 0

        if delta_ns < 1_000:
            s = f"+{delta_ns}ns"
        elif delta_ns < 1_000_000:
            s = f"+{delta_ns / 1_000:.0f}µs"
        elif delta_ns < 1_000_000_000:
            ms = delta_ns / 1_000_000
            s = f"+{ms:.1f}ms" if ms < 10 else f"+{ms:.0f}ms"
        else:
            sec = delta_ns / 1_000_000_000
            s = f"+{sec:.1f}s" if sec < 10 else f"+{sec:.0f}s"

        if len(s) > 10:
            s = s[:10]
        return s.rjust(10)

    def _get_detail_text(self) -> str:
        with self._lock:
            view = self._get_filtered_view_locked()
            if not view or not (0 <= self._selected < len(view)):
                return ""
            ev = view[self._selected]

            detail = [
                f"id={ev.id}",
                f"pid={ev.pid} tid={ev.tid} abi={ev.abi}",
                f"nr={ev.nr} name={ev.name}",
                f"enter_time_ns={ev.time_ns}",
                f"map_gen={ev.map_gen} stack_id={ev.stack_id}",
                "",
            ]

            if ev.exit_time_ns is not None:
                detail.append(f"exit_time_ns={ev.exit_time_ns}")
                if ev.exit_retval is not None:
                    detail.append(f"retval={ev.exit_retval}")
                detail.append("status=FAILED" if ev.failed else "status=OK")
                detail.append("")

            if ev.stack_id < 0:
                detail.append("(no stack)")
            elif ev.resolving:
                detail.append("Resolving backtrace…")
            elif ev.resolve_error is not None:
                detail.append(f"Resolve failed: {ev.resolve_error}")
            elif ev.stack is None:
                detail.append("Press Enter to resolve backtrace")
            else:
                detail.append("Call stack:")
                detail += self._format_call_stack(ev)

            return "\n".join(detail)

    def _get_status_text(self) -> str:
        with self._lock:
            view = self._get_filtered_view_locked()
            mode = "tail" if self._tailing else "paused (press 't')"
            pieces = [f"events={len(view)}"]
            if view:
                pieces.append(f"selected={self._selected+1}/{len(view)}")
            pieces.append(f"[{mode}]")

            if self._status_message:
                pieces.append(self._status_message)

            if self._search_text != "":
                pieces.append(f"search={json.dumps(self._search_text, ensure_ascii=False)}")
                if self._search_matches:
                    pieces.append(f"match={self._search_index+1}/{len(self._search_matches)}")

            if self._filter_text != "":
                pieces.append(f"filter={json.dumps(self._filter_text, ensure_ascii=False)} (Ctrl-L clears)")

            if self._search_text != "":
                pieces.append("(f = filter search)")

            if self._excluded_syscalls:
                pieces.append(f"excluded={len(self._excluded_syscalls)} (x=exclude)")

            cols = get_app().output.get_size().columns
            if self._show_details:
                placement = "below" if cols < self._details_breakpoint_cols else "right"
                pieces.append(f"details=on ({placement})  d=hide")
            else:
                pieces.append("details=off  d=show")

            pieces.append("?=help")

            return "  ".join(pieces)

    def _format_call_stack(self, ev: SyscallEvent) -> List[str]:
        modules = ev.symbols["modules"]
        entries = ev.symbols["symbols"]
        stack = ev.stack

        out: List[str] = []
        for addr, (mod_index, offset) in zip(stack, entries):
            if mod_index == 0xFFFFFFFF:
                out.append(f"  0x{addr:x}")
            else:
                path, *extra = modules[mod_index]
                base = path.rsplit("/", 1)[-1]
                out.append(f"  {base}+0x{offset:x}")
        for addr in stack[len(entries) :]:
            out.append(f"  0x{addr:x}")
        return out


class SyscallListControl(UIControl):
    def __init__(self, model: ListModel) -> None:
        self._model = model

    def is_focusable(self) -> bool:
        return True

    def create_content(self, width: int, height: int) -> UIContent:
        m = self._model
        snap = m.snapshot()
        m.prepare(snap)

        view = snap.view
        if not view:

            def get_line(i: int) -> FormattedText:
                return [("", "(no events)")]

            return UIContent(get_line=get_line, line_count=1, cursor_position=Point(0, 0))

        cursor_y = max(0, min(snap.selected, len(view) - 1))

        def get_line(i: int) -> FormattedText:
            return m.get_line(snap, i)

        return UIContent(get_line=get_line, line_count=len(view), cursor_position=Point(0, cursor_y))


@dataclass(frozen=True)
class ListSnapshot:
    cache_key: tuple
    view: List[SyscallEvent]
    selected: int
    search_text: str


class ListModel:
    def __init__(
        self,
        snapshot: Callable[[], ListSnapshot],
        format_dt: Callable[[int], str],
        format_line: Callable[[SyscallEvent, str], FormattedText],
        highlight_line: Callable[[FormattedText, str], FormattedText],
    ) -> None:
        self._snapshot = snapshot
        self._format_dt = format_dt
        self._format_line = format_line
        self._highlight_line = highlight_line

        self._dt_cache_key: Optional[tuple] = None
        self._dt_cache_len: int = -1
        self._dt_cache: List[str] = []

        self._line_cache_key: Optional[tuple] = None
        self._line_cache: Dict[int, FormattedText] = {}

    def snapshot(self) -> ListSnapshot:
        return self._snapshot()

    def prepare(self, snap: ListSnapshot) -> None:
        if self._line_cache_key != snap.cache_key:
            self._line_cache_key = snap.cache_key
            self._line_cache.clear()

        if self._dt_cache_key == snap.cache_key and self._dt_cache_len == len(snap.view):
            return

        prev_ns_by_thread: Dict[Tuple[int, int], int] = {}
        dt_list: List[str] = []
        for ev in snap.view:
            thread_key = (ev.pid, ev.tid)
            prev_ns = prev_ns_by_thread.get(thread_key)
            if prev_ns is None:
                dt = " " * 10
            else:
                dt = self._format_dt(ev.time_ns - prev_ns)
            prev_ns_by_thread[thread_key] = ev.time_ns
            dt_list.append(dt)

        self._dt_cache_key = snap.cache_key
        self._dt_cache_len = len(snap.view)
        self._dt_cache = dt_list

    def get_line(self, snap: ListSnapshot, i: int) -> FormattedText:
        view = snap.view
        if i < 0 or i >= len(view):
            return [("", "")]

        cached = self._line_cache.get(i)
        if cached is not None:
            return cached

        dt = self._dt_cache[i]
        line = self._format_line(view[i], dt)

        needle_lc = snap.search_text.lower()
        if needle_lc:
            line = self._highlight_line(line, needle_lc)

        if len(self._line_cache) > 2000:
            self._line_cache.clear()
        self._line_cache[i] = line
        return line


class SyscallTracer:
    def __init__(self, reactor: Reactor) -> None:
        self._notify: Optional[callable] = None
        self._platform: Optional[str] = None
        self._reactor = reactor

        self._service: Optional[frida.core.Service] = None

        self._signatures: Dict[str, Dict[int, Dict[str, Any]]] = {}
        self._abi_by_pid: Dict[int, str] = {}

        self._events_out: "queue.Queue[SyscallEvent]" = queue.Queue()
        self._updates_out: "queue.Queue[tuple[int, Optional[list[int]], Any]]" = queue.Queue()

        self._next_id = 1
        self._excluded: set[tuple[str, int]] = set()
        self._stopping = False

        self._schedule_on_message = lambda m: self._reactor.schedule(lambda: self._handle_service_message(m))

    def set_notify(self, notify: callable) -> None:
        self._notify = notify

    def set_platform(self, platform: Optional[str]) -> None:
        self._platform = platform

    def start(
        self,
        device: frida.core.Device,
        targets_req: Dict[str, Any],
        excluded_by_name: Optional[list[str]],
        included_by_name: Optional[list[str]] = None,
    ) -> None:
        self._service = device.open_service("syscall-trace")

        raw = self._service.request({"type": "get-signatures"})
        sigs: Dict[str, Dict[int, Dict[str, Any]]] = {}
        for abi, entries in raw.items():
            by_nr: Dict[int, Dict[str, Any]] = {}
            for nr, name, args in entries:
                by_nr[nr] = {"name": name, "args": args}
            sigs[abi] = by_nr
        self._signatures = sigs

        self._service.on("message", self._schedule_on_message)

        if included_by_name:
            included = set(self._resolve_excludes_by_name(included_by_name))
            to_exclude = []
            for abi, by_nr in self._signatures.items():
                for nr in by_nr:
                    if (abi, nr) not in included:
                        to_exclude.append((abi, nr))
            if to_exclude:
                by_abi = self._apply_excludes(to_exclude)
                self._service.request({"type": "exclude-syscalls", **by_abi})
        elif excluded_by_name:
            items = self._resolve_excludes_by_name(excluded_by_name)
            if items:
                by_abi = self._apply_excludes(items)
                self._service.request({"type": "exclude-syscalls", **by_abi})

        add_req = {"type": "add-targets"}
        add_req.update(targets_req)
        self._service.request(add_req)

    def stop(self) -> None:
        self._stopping = True
        if self._service is not None:
            self._service.off("message", self._schedule_on_message)
            self._service.close()
        self._service = None

    def drain_events(self, limit: int = 2000) -> List[SyscallEvent]:
        out: List[SyscallEvent] = []
        for _ in range(limit):
            try:
                out.append(self._events_out.get_nowait())
            except queue.Empty:
                break
        return out

    def drain_updates(self, limit: int = 2000) -> List[tuple[int, Optional[list[int]], Any]]:
        out: List[tuple[int, Optional[list[int]], Any]] = []
        for _ in range(limit):
            try:
                out.append(self._updates_out.get_nowait())
            except queue.Empty:
                break
        return out

    def resolve_backtrace(self, event_id: int, pid: int, map_gen: int, stack_id: int) -> None:
        self._reactor.schedule(lambda: self._resolve_backtrace_on_reactor(event_id, pid, map_gen, stack_id))

    def get_excluded(self) -> set[tuple[str, int]]:
        return self._excluded

    def exclude_syscalls(self, items: list[tuple[str, int]]) -> None:
        by_abi = self._apply_excludes(items)
        self._reactor.schedule(lambda: self._exclude_syscalls_on_reactor(by_abi))

    def _exclude_syscalls_on_reactor(self, by_abi: dict[str, list[int]]) -> None:
        if self._stopping or self._service is None:
            return
        self._service.request({"type": "exclude-syscalls", **by_abi})

    def _group_by_abi(self, items: set[tuple[str, int]] | list[tuple[str, int]]) -> dict[str, list[int]]:
        by_abi: dict[str, set[int]] = {}
        for abi, nr in items:
            by_abi.setdefault(abi, set()).add(nr)
        return {abi: sorted(nrs) for abi, nrs in by_abi.items()}

    def _apply_excludes(self, items: set[tuple[str, int]] | list[tuple[str, int]]) -> dict[str, list[int]]:
        for k in items:
            self._excluded.add(k)
        return self._group_by_abi(items)

    def _resolve_excludes_by_name(self, names: list[str]) -> list[tuple[str, int]]:
        wanted = [n.lower() for n in names if n]
        out: list[tuple[str, int]] = []
        for abi, by_nr in self._signatures.items():
            for nr, sig in by_nr.items():
                sname = sig["name"].lower()
                if sname in wanted:
                    out.append((abi, nr))
        seen = set()
        uniq = []
        for k in out:
            if k not in seen:
                seen.add(k)
                uniq.append(k)
        return uniq

    def _handle_service_message(self, m) -> None:
        if self._stopping or self._service is None:
            return
        if m["type"] != "events-available":
            return

        self._reactor.schedule(self._read_events_loop)

    def _read_events_loop(self) -> None:
        if self._stopping or self._service is None:
            return

        while True:
            res = self._service.request({"type": "read-events"})
            events = res["events"]
            processes = res["processes"]
            status = res["status"]

            for pid, abi in processes:
                self._abi_by_pid[pid] = abi

            for row in events:
                ev = self._parse_event_row(row)

                if (ev.abi, ev.nr) in self._excluded:
                    continue

                self._events_out.put(ev)

            if events:
                self._notify_ui()

            if status != "more":
                break

        self._notify_ui()

    def _notify_ui(self) -> None:
        if self._notify is not None:
            self._notify()

    def _parse_event_row(self, row) -> SyscallEvent:
        phase, time_ns, tid, pid, nr, stack_id, map_gen, args_or_retval, attachments = row

        abi = self._abi_by_pid[pid]

        name = "some syscall"
        sig_args = None
        if nr != -1:
            sig = self._signatures[abi].get(nr)
            if sig is not None:
                name = sig["name"]
                sig_args = sig["args"]
            else:
                name = f"#{nr}"

        if phase == "enter":
            raw_args = self._apply_attachments(args_or_retval, attachments)

            args: List[Arg] = []
            if sig_args is not None:
                for i, (atype, aname) in enumerate(sig_args):
                    if i >= len(raw_args):
                        break
                    args.append(self._make_arg(name, atype, aname, raw_args[i]))
            else:
                for i, v in enumerate(raw_args):
                    args.append(self._make_arg(name, "", f"arg{i}", v))

            ev = SyscallEvent(
                id=self._next_id,
                phase="enter",
                time_ns=time_ns,
                pid=pid,
                tid=tid,
                nr=nr,
                name=name,
                stack_id=stack_id,
                map_gen=map_gen,
                abi=abi,
                enter_args=args,
                enter_summary=None,
            )
        else:
            retval = args_or_retval
            failed = retval < 0

            out_args: Optional[List[Arg]] = None
            if attachments:
                out: List[Arg] = []
                for ai, av in attachments:
                    if sig_args is not None:
                        atype, aname = sig_args[ai]
                    else:
                        atype, aname = "", f"arg{ai}"
                    out.append(self._make_arg(name, atype, aname, av))
                out_args = out

            ev = SyscallEvent(
                id=self._next_id,
                phase="exit",
                time_ns=time_ns,
                pid=pid,
                tid=tid,
                nr=nr,
                name=name,
                stack_id=stack_id,
                map_gen=map_gen,
                abi=abi,
                enter_args=None,
                enter_summary=None,
                exit_retval=retval,
                exit_out_args=out_args,
                exit_time_ns=time_ns,
                failed=failed,
            )

        self._next_id += 1
        return ev

    def _apply_attachments(self, raw_args: list[Any], attachments: list[tuple[int, Any]]) -> list[Any]:
        if not attachments:
            return raw_args

        out = list(raw_args)
        for ai, av in attachments:
            out[ai] = av
        return out

    def _make_arg(self, syscall_name: str, atype: str, aname: str, value: Any) -> Arg:
        is_linux = self._platform == "linux"

        if is_linux:
            if aname == "sig" and atype == "int":
                txt = linux_signal_text(value)
                return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
            if syscall_name in ("futex", "futex_time64"):
                if aname == "op":
                    txt = linux_futex_op_text(value)
                    return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
                if aname == "val":
                    txt = linux_futex_val_text(value)
                    return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
            if syscall_name in ("mmap", "mprotect") and aname == "prot":
                txt = linux_prot_text(value)
                return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
            if syscall_name == "mmap" and aname == "flags":
                txt = linux_mmap_flags_text(value)
                return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
            if syscall_name == "mremap" and aname == "flags":
                txt = linux_mremap_flags_text(value)
                return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")
            if syscall_name == "clone" and aname == "clone_flags":
                txt = linux_clone_flags_text(value)
                return Arg(type=atype, name=aname, value=value, text=txt, is_fd=False, kind="other")

        if (atype.endswith("*") or atype == "unsigned long") and isinstance(value, int):
            if value == 0:
                return Arg(type=atype, name=aname, value=value, text="NULL", is_fd=False, kind="ptr")
            return Arg(type=atype, name=aname, value=value, text=f"0x{value:x}", is_fd=False, kind="ptr")

        is_fd = (atype in ("int", "unsigned int")) and aname.endswith("fd")
        if is_fd:
            fd = to_signed32(value)
            if is_linux and fd == LINUX_AT_FDCWD:
                return Arg(type=atype, name=aname, value=fd, text="AT_FDCWD", is_fd=True, kind="fd")
            return Arg(type=atype, name=aname, value=fd, text=str(fd), is_fd=True, kind="fd")

        if isinstance(value, str):
            s = value if len(value) <= 140 else (value[:137] + "…")
            return Arg(
                type=atype,
                name=aname,
                value=value,
                text=json.dumps(s, ensure_ascii=False),
                is_fd=is_fd,
                kind="string",
            )

        if isinstance(value, (bytes, bytearray)):
            b = bytes(value)
            if len(b) <= 32:
                text = b.hex()
            else:
                text = b[:32].hex() + f"…({len(b)} bytes)"
            return Arg(type=atype, name=aname, value=value, text=text, is_fd=is_fd, kind="bytes")

        return Arg(type=atype, name=aname, value=value, text=repr(value), is_fd=is_fd, kind="other")

    def _resolve_backtrace_on_reactor(self, event_id: int, pid: int, map_gen: int, stack_id: int) -> None:
        if self._stopping or self._service is None:
            return
        try:
            stacks_res = self._service.request({"type": "resolve-stacks", "ids": [stack_id]})
            stack = stacks_res["stacks"][0]

            syms = self._service.request(
                {
                    "type": "resolve-symbols",
                    "pid": pid,
                    "gen": map_gen,
                    "addresses": [("uint64", address) for address in stack],
                }
            )

            self._updates_out.put((event_id, stack, syms))
        except Exception as e:
            self._updates_out.put((event_id, None, e))
        finally:
            self._notify_ui()


@dataclass
class SyscallEvent:
    id: int
    phase: str
    time_ns: int
    pid: int
    tid: int
    nr: int
    name: str

    stack_id: int
    map_gen: int
    abi: str

    enter_args: Optional[List[Arg]] = None
    enter_summary: Optional[str] = None

    exit_retval: Optional[Any] = None
    exit_out_args: Optional[List[Arg]] = None
    exit_time_ns: Optional[int] = None

    merged: bool = False
    failed: bool = False
    resolving: bool = False
    stack: Optional[List[int]] = None
    symbols: Optional[Any] = None
    resolve_error: Optional[str] = None

    def set_exit(self, retval: int, out_args: Optional[List[Arg]], exit_time_ns: int, stack_id: int) -> None:
        self.exit_retval = retval
        self.exit_out_args = out_args
        self.exit_time_ns = exit_time_ns
        if self.stack_id == -1:
            self.stack_id = stack_id
        self.merged = True
        self.failed = retval < 0


@dataclass
class Arg:
    type: str
    name: str
    value: Any
    text: str
    is_fd: bool
    kind: str


LINUX_AT_FDCWD = -100

LINUX_FD_RETURNING_SYSCALLS = {
    "open",
    "openat",
    "openat2",
    "creat",
    "socket",
    "socketpair",
    "accept",
    "accept4",
    "dup",
    "dup2",
    "dup3",
    "pipe",
    "pipe2",
    "eventfd",
    "eventfd2",
    "inotify_init",
    "inotify_init1",
    "memfd_create",
    "signalfd",
    "signalfd4",
    "timerfd_create",
    "pidfd_open",
    "epoll_create",
    "epoll_create1",
}

LINUX_ERRNO: Dict[int, str] = {
    1: "EPERM",
    2: "ENOENT",
    3: "ESRCH",
    4: "EINTR",
    5: "EIO",
    6: "ENXIO",
    7: "E2BIG",
    8: "ENOEXEC",
    9: "EBADF",
    10: "ECHILD",
    11: "EAGAIN",
    12: "ENOMEM",
    13: "EACCES",
    14: "EFAULT",
    15: "ENOTBLK",
    16: "EBUSY",
    17: "EEXIST",
    18: "EXDEV",
    19: "ENODEV",
    20: "ENOTDIR",
    21: "EISDIR",
    22: "EINVAL",
    23: "ENFILE",
    24: "EMFILE",
    25: "ENOTTY",
    26: "ETXTBSY",
    27: "EFBIG",
    28: "ENOSPC",
    29: "ESPIPE",
    30: "EROFS",
    31: "EMLINK",
    32: "EPIPE",
    33: "EDOM",
    34: "ERANGE",
    35: "EDEADLK",
    36: "ENAMETOOLONG",
    37: "ENOLCK",
    38: "ENOSYS",
    39: "ENOTEMPTY",
    40: "ELOOP",
    42: "ENOMSG",
    43: "EIDRM",
    44: "ECHRNG",
    45: "EL2NSYNC",
    46: "EL3HLT",
    47: "EL3RST",
    48: "ELNRNG",
    49: "EUNATCH",
    50: "ENOCSI",
    51: "EL2HLT",
    52: "EBADE",
    53: "EBADR",
    54: "EXFULL",
    55: "ENOANO",
    56: "EBADRQC",
    57: "EBADSLT",
    59: "EBFONT",
    60: "ENOSTR",
    61: "ENODATA",
    62: "ETIME",
    63: "ENOSR",
    64: "ENONET",
    65: "ENOPKG",
    66: "EREMOTE",
    67: "ENOLINK",
    68: "EADV",
    69: "ESRMNT",
    70: "ECOMM",
    71: "EPROTO",
    72: "EMULTIHOP",
    73: "EDOTDOT",
    74: "EBADMSG",
    75: "EOVERFLOW",
    76: "ENOTUNIQ",
    77: "EBADFD",
    78: "EREMCHG",
    79: "ELIBACC",
    80: "ELIBBAD",
    81: "ELIBSCN",
    82: "ELIBMAX",
    83: "ELIBEXEC",
    84: "EILSEQ",
    85: "ERESTART",
    86: "ESTRPIPE",
    87: "EUSERS",
    88: "ENOTSOCK",
    89: "EDESTADDRREQ",
    90: "EMSGSIZE",
    91: "EPROTOTYPE",
    92: "ENOPROTOOPT",
    93: "EPROTONOSUPPORT",
    94: "ESOCKTNOSUPPORT",
    95: "EOPNOTSUPP",
    96: "EPFNOSUPPORT",
    97: "EAFNOSUPPORT",
    98: "EADDRINUSE",
    99: "EADDRNOTAVAIL",
    100: "ENETDOWN",
    101: "ENETUNREACH",
    102: "ENETRESET",
    103: "ECONNABORTED",
    104: "ECONNRESET",
    105: "ENOBUFS",
    106: "EISCONN",
    107: "ENOTCONN",
    108: "ESHUTDOWN",
    109: "ETOOMANYREFS",
    110: "ETIMEDOUT",
    111: "ECONNREFUSED",
    112: "EHOSTDOWN",
    113: "EHOSTUNREACH",
    114: "EALREADY",
    115: "EINPROGRESS",
    116: "ESTALE",
    117: "EUCLEAN",
    118: "ENOTNAM",
    119: "ENAVAIL",
    120: "EISNAM",
    121: "EREMOTEIO",
    122: "EDQUOT",
    123: "ENOMEDIUM",
    124: "EMEDIUMTYPE",
    125: "ECANCELED",
    126: "ENOKEY",
    127: "EKEYEXPIRED",
    128: "EKEYREVOKED",
    129: "EKEYREJECTED",
    130: "EOWNERDEAD",
    131: "ENOTRECOVERABLE",
    132: "ERFKILL",
    133: "EHWPOISON",
    512: "ERESTARTSYS",
    513: "ERESTARTNOINTR",
    514: "ERESTARTNOHAND",
    515: "ENOIOCTLCMD",
    516: "ERESTART_RESTARTBLOCK",
}

LINUX_SIGNAL_NAMES: Dict[int, str] = {
    1: "SIGHUP",
    2: "SIGINT",
    3: "SIGQUIT",
    4: "SIGILL",
    5: "SIGTRAP",
    6: "SIGABRT",
    7: "SIGBUS",
    8: "SIGFPE",
    9: "SIGKILL",
    10: "SIGUSR1",
    11: "SIGSEGV",
    12: "SIGUSR2",
    13: "SIGPIPE",
    14: "SIGALRM",
    15: "SIGTERM",
    16: "SIGSTKFLT",
    17: "SIGCHLD",
    18: "SIGCONT",
    19: "SIGSTOP",
    20: "SIGTSTP",
    21: "SIGTTIN",
    22: "SIGTTOU",
    23: "SIGURG",
    24: "SIGXCPU",
    25: "SIGXFSZ",
    26: "SIGVTALRM",
    27: "SIGPROF",
    28: "SIGWINCH",
    29: "SIGIO",
    30: "SIGPWR",
    31: "SIGSYS",
}

LINUX_FUTEX_CMD_MASK = 0x7F
LINUX_FUTEX_PRIVATE_FLAG = 0x80
LINUX_FUTEX_CLOCK_REALTIME = 0x100

LINUX_FUTEX_OPS: Dict[int, str] = {
    0: "FUTEX_WAIT",
    1: "FUTEX_WAKE",
    2: "FUTEX_FD",
    3: "FUTEX_REQUEUE",
    4: "FUTEX_CMP_REQUEUE",
    5: "FUTEX_WAKE_OP",
    6: "FUTEX_LOCK_PI",
    7: "FUTEX_UNLOCK_PI",
    8: "FUTEX_TRYLOCK_PI",
    9: "FUTEX_WAIT_BITSET",
    10: "FUTEX_WAKE_BITSET",
    11: "FUTEX_WAIT_REQUEUE_PI",
    12: "FUTEX_CMP_REQUEUE_PI",
}

LINUX_PROT_FLAGS: Dict[int, str] = {
    0x1: "PROT_READ",
    0x2: "PROT_WRITE",
    0x4: "PROT_EXEC",
    0x01000000: "PROT_GROWSDOWN",
    0x02000000: "PROT_GROWSUP",
}

LINUX_MMAP_FLAGS: Dict[int, str] = {
    0x01: "MAP_SHARED",
    0x02: "MAP_PRIVATE",
    0x03: "MAP_SHARED_VALIDATE",
    0x0F: "MAP_TYPE",
    0x10: "MAP_FIXED",
    0x20: "MAP_ANONYMOUS",
    0x0100: "MAP_GROWSDOWN",
    0x0800: "MAP_DENYWRITE",
    0x1000: "MAP_EXECUTABLE",
    0x2000: "MAP_LOCKED",
    0x4000: "MAP_NORESERVE",
    0x8000: "MAP_POPULATE",
    0x10000: "MAP_NONBLOCK",
    0x20000: "MAP_STACK",
    0x40000: "MAP_HUGETLB",
    0x80000: "MAP_SYNC",
    0x100000: "MAP_FIXED_NOREPLACE",
}

LINUX_MREMAP_FLAGS: Dict[int, str] = {
    0x1: "MREMAP_MAYMOVE",
    0x2: "MREMAP_FIXED",
    0x4: "MREMAP_DONTUNMAP",
}

LINUX_CLONE_FLAGS: Dict[int, str] = {
    0x00000100: "CLONE_VM",
    0x00000200: "CLONE_FS",
    0x00000400: "CLONE_FILES",
    0x00000800: "CLONE_SIGHAND",
    0x00001000: "CLONE_PIDFD",
    0x00002000: "CLONE_PTRACE",
    0x00004000: "CLONE_VFORK",
    0x00008000: "CLONE_PARENT",
    0x00010000: "CLONE_THREAD",
    0x00020000: "CLONE_NEWNS",
    0x00040000: "CLONE_SYSVSEM",
    0x00080000: "CLONE_SETTLS",
    0x00100000: "CLONE_PARENT_SETTID",
    0x00200000: "CLONE_CHILD_CLEARTID",
    0x00400000: "CLONE_DETACHED",
    0x00800000: "CLONE_UNTRACED",
    0x01000000: "CLONE_CHILD_SETTID",
    0x02000000: "CLONE_NEWCGROUP",
    0x04000000: "CLONE_NEWUTS",
    0x08000000: "CLONE_NEWIPC",
    0x10000000: "CLONE_NEWUSER",
    0x20000000: "CLONE_NEWPID",
    0x40000000: "CLONE_NEWNET",
    0x80000000: "CLONE_IO",
    0x00000080: "CLONE_NEWTIME",
    0x100000000: "CLONE_CLEAR_SIGHAND",
    0x200000000: "CLONE_INTO_CGROUP",
}


def linux_signal_text(n: int) -> str:
    name = LINUX_SIGNAL_NAMES.get(n)
    return name if name is not None else f"SIG{n}"


def linux_futex_op_text(v: int) -> str:
    cmd = v & LINUX_FUTEX_CMD_MASK
    flags = v & ~LINUX_FUTEX_CMD_MASK

    base = LINUX_FUTEX_OPS.get(cmd, f"0x{cmd:x}")
    if flags == 0:
        return base

    parts: List[str] = [base]
    if flags & LINUX_FUTEX_PRIVATE_FLAG:
        parts.append("FUTEX_PRIVATE_FLAG")
        flags &= ~LINUX_FUTEX_PRIVATE_FLAG
    if flags & LINUX_FUTEX_CLOCK_REALTIME:
        parts.append("FUTEX_CLOCK_REALTIME")
        flags &= ~LINUX_FUTEX_CLOCK_REALTIME
    if flags:
        parts.append(f"0x{flags:x}")
    return "|".join(parts)


def linux_futex_val_text(v: int) -> str:
    return str(to_signed32(v))


def linux_prot_text(v: int) -> str:
    if v == 0:
        return "PROT_NONE"
    return format_bitmask(v, {k: n for k, n in LINUX_PROT_FLAGS.items() if k != 0})


def linux_mmap_flags_text(v: int) -> str:
    return format_bitmask(v, LINUX_MMAP_FLAGS)


def linux_mremap_flags_text(v: int) -> str:
    return format_bitmask(v, LINUX_MREMAP_FLAGS)


def linux_clone_flags_text(v: int) -> str:
    sig = v & 0x7F
    flags_part = v & ~0xFF

    if v & 0x80:
        flags_part |= 0x80

    parts: List[str] = []
    if flags_part:
        s = format_bitmask(flags_part, LINUX_CLONE_FLAGS)
        if s != "0":
            parts.append(s)

    if sig:
        parts.append(linux_signal_text(sig))

    return "|".join(parts) if parts else "0"


def format_bitmask(v: int, names: Dict[int, str]) -> str:
    parts: List[str] = []
    remaining = v
    for bit, name in sorted(names.items()):
        if bit != 0 and (v & bit) == bit:
            parts.append(name)
            remaining &= ~bit
    if remaining:
        parts.append(f"0x{remaining:x}")
    return "|".join(parts) if parts else f"0x{v:x}"


def to_signed32(v: int) -> int:
    v &= 0xFFFFFFFF
    if v & 0x80000000:
        return v - 0x100000000
    return v


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
