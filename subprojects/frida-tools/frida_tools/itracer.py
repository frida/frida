import json
import os
import struct
from abc import ABC, abstractmethod
from pathlib import Path
from typing import List, Optional, Sequence, Tuple, TypeVar, Union

import frida
from frida.core import RPCException

from frida_tools.reactor import Reactor

CodeLocation = Union[
    Tuple[str, str],
    Tuple[str, Tuple[str, str]],
    Tuple[str, Tuple[str, int]],
]

TraceThreadStrategy = Tuple[str, Tuple[str, int]]
TraceRangeStrategy = Tuple[str, Tuple[CodeLocation, Optional[CodeLocation]]]
TraceStrategy = Union[TraceThreadStrategy, TraceRangeStrategy]


def main() -> None:
    import argparse
    import threading

    from prompt_toolkit import PromptSession, prompt
    from prompt_toolkit.application import Application
    from prompt_toolkit.formatted_text import AnyFormattedText, FormattedText
    from prompt_toolkit.key_binding.defaults import load_key_bindings
    from prompt_toolkit.key_binding.key_bindings import KeyBindings, merge_key_bindings
    from prompt_toolkit.layout import Layout
    from prompt_toolkit.layout.containers import HSplit
    from prompt_toolkit.styles import BaseStyle
    from prompt_toolkit.widgets import Label, RadioList

    from frida_tools.application import ConsoleApplication

    class InstructionTracerApplication(ConsoleApplication, InstructionTracerUI):
        _itracer: Optional[InstructionTracer]

        def __init__(self) -> None:
            self._state = "starting"
            self._ready = threading.Event()
            self._cli = PromptSession()
            super().__init__(self._process_input)

        def _add_options(self, parser: argparse.ArgumentParser) -> None:
            parser.add_argument(
                "-t", "--thread-id", help="trace THREAD_ID", metavar="THREAD_ID", dest="strategy", type=parse_thread_id
            )
            parser.add_argument(
                "-i",
                "--thread-index",
                help="trace THREAD_INDEX",
                metavar="THREAD_INDEX",
                dest="strategy",
                type=parse_thread_index,
            )
            parser.add_argument(
                "-r",
                "--range",
                help="trace RANGE, e.g.: 0x1000..0x1008, libc.so!sleep, libc.so!0x1234, recv..memcpy",
                metavar="RANGE",
                dest="strategy",
                type=parse_range,
            )
            parser.add_argument("-o", "--output", help="output to file", dest="outpath")

        def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
            self._itracer = None
            self._strategy = options.strategy
            self._outpath = options.outpath

        def _usage(self) -> str:
            return "%(prog)s [options] target"

        def _needs_target(self) -> bool:
            return True

        def _start(self) -> None:
            self._update_status("Injecting script...")
            self._itracer = InstructionTracer(self._reactor)
            self._itracer.start(self._device, self._session, self._runtime, self)
            self._ready.set()

        def _stop(self) -> None:
            assert self._itracer is not None
            self._itracer.dispose()
            self._itracer = None

            try:
                self._cli.app.exit()
            except:
                pass

        def _process_input(self, reactor: Reactor) -> None:
            try:
                while self._ready.wait(0.5) != True:
                    if not reactor.is_running():
                        return
            except KeyboardInterrupt:
                reactor.cancel_io()
                return

            if self._state != "started":
                return

            try:
                self._cli.prompt()
            except:
                pass

        def get_trace_strategy(self) -> Optional[TraceStrategy]:
            return self._strategy

        def prompt_for_trace_strategy(self, threads: List[dict]) -> Optional[TraceStrategy]:
            kind = radiolist_prompt(
                title="Tracing strategy:",
                values=[
                    ("thread", "Thread"),
                    ("range", "Range"),
                ],
            )
            if kind is None:
                raise KeyboardInterrupt

            if kind == "thread":
                thread_id = radiolist_prompt(
                    title="Running threads:", values=[(t["id"], json.dumps(t)) for t in threads]
                )
                if thread_id is None:
                    raise KeyboardInterrupt
                return ("thread", ("id", thread_id))

            while True:
                try:
                    text = prompt("Start address: ").strip()
                    if len(text) == 0:
                        continue
                    start = parse_code_location(text)
                    break
                except Exception as e:
                    print(str(e))
                    continue

            while True:
                try:
                    text = prompt("End address (optional): ").strip()
                    if len(text) > 0:
                        end = parse_code_location(text)
                    else:
                        end = None
                    break
                except Exception as e:
                    print(str(e))
                    continue

            return ("range", (start, end))

        def get_trace_output_path(self, suggested_name: Optional[str] = None) -> os.PathLike:
            return self._outpath

        def prompt_for_trace_output_path(self, suggested_name: str) -> Optional[os.PathLike]:
            while True:
                outpath = prompt("Output filename: ", default=suggested_name).strip()
                if len(outpath) != 0:
                    break
            return outpath

        def on_trace_started(self) -> None:
            self._state = "started"

        def on_trace_stopped(self, error_message: Optional[str] = None) -> None:
            self._state = "stopping"

            if error_message is not None:
                self._log(level="error", text=error_message)
                self._exit(1)
            else:
                self._exit(0)

            try:
                self._cli.app.exit()
            except:
                pass

        def on_trace_progress(self, total_blocks: int, total_bytes: int) -> None:
            blocks_suffix = "s" if total_blocks != 1 else ""
            self._cli.message = FormattedText(
                [
                    ("bold", "Tracing!"),
                    ("", " Collected "),
                    ("fg:green bold", human_readable_size(total_bytes)),
                    ("", f" from {total_blocks} basic block{blocks_suffix}"),
                ]
            )
            self._cli.app.invalidate()

    def parse_thread_id(value: str) -> TraceThreadStrategy:
        return ("thread", ("id", int(value)))

    def parse_thread_index(value: str) -> TraceThreadStrategy:
        return ("thread", ("index", int(value)))

    def parse_range(value: str) -> TraceRangeStrategy:
        tokens = value.split("..", 1)
        start = tokens[0]
        end = tokens[1] if len(tokens) == 2 else None
        return ("range", (parse_code_location(start), parse_code_location(end)))

    def parse_code_location(value: Optional[str]) -> CodeLocation:
        if value is None:
            return None

        if value.startswith("0x"):
            return ("address", value)

        tokens = value.split("!", 1)
        if len(tokens) == 2:
            name = tokens[0]
            subval = tokens[1]
            if subval.startswith("0x"):
                return ("module-offset", (name, int(subval, 16)))
            return ("module-export", (name, subval))

        return ("symbol", tokens[0])

    # Based on https://stackoverflow.com/a/43690506
    def human_readable_size(size):
        for unit in ["B", "KiB", "MiB", "GiB"]:
            if size < 1024.0 or unit == "GiB":
                break
            size /= 1024.0
        return f"{size:.2f} {unit}"

    T = TypeVar("T")

    # Based on https://github.com/prompt-toolkit/python-prompt-toolkit/issues/756#issuecomment-1294742392
    def radiolist_prompt(
        title: str = "",
        values: Sequence[Tuple[T, AnyFormattedText]] = None,
        default: Optional[T] = None,
        cancel_value: Optional[T] = None,
        style: Optional[BaseStyle] = None,
    ) -> T:
        radio_list = RadioList(values, default)
        radio_list.control.key_bindings.remove("enter")

        bindings = KeyBindings()

        @bindings.add("enter")
        def exit_with_value(event):
            radio_list._handle_enter()
            event.app.exit(result=radio_list.current_value)

        @bindings.add("c-c")
        def backup_exit_with_value(event):
            event.app.exit(result=cancel_value)

        application = Application(
            layout=Layout(HSplit([Label(title), radio_list])),
            key_bindings=merge_key_bindings([load_key_bindings(), bindings]),
            mouse_support=True,
            style=style,
            full_screen=False,
        )
        return application.run()

    app = InstructionTracerApplication()
    app.run()


class InstructionTracerUI(ABC):
    @abstractmethod
    def get_trace_strategy(self) -> Optional[TraceStrategy]:
        raise NotImplementedError

    def prompt_for_trace_strategy(self, threads: List[dict]) -> Optional[TraceStrategy]:
        return None

    @abstractmethod
    def get_trace_output_path(self) -> Optional[os.PathLike]:
        raise NotImplementedError

    def prompt_for_trace_output_path(self, suggested_name: str) -> Optional[os.PathLike]:
        return None

    @abstractmethod
    def on_trace_started(self) -> None:
        raise NotImplementedError

    @abstractmethod
    def on_trace_stopped(self, error_message: Optional[str] = None) -> None:
        raise NotImplementedError

    def on_trace_progress(self, total_blocks: int, total_bytes: int) -> None:
        pass

    def _on_script_created(self, script: frida.core.Script) -> None:
        pass


class InstructionTracer:
    FILE_MAGIC = b"ITRC"

    def __init__(self, reactor: Reactor) -> None:
        self._reactor = reactor
        self._outfile = None
        self._ui: Optional[InstructionTracerUI] = None
        self._total_blocks = 0
        self._tracer_script: Optional[frida.core.Script] = None
        self._reader_script: Optional[frida.core.Script] = None
        self._reader_api = None

    def dispose(self) -> None:
        if self._reader_api is not None:
            try:
                self._reader_api.stop_buffer_reader()
            except:
                pass
            self._reader_api = None

        if self._reader_script is not None:
            try:
                self._reader_script.unload()
            except:
                pass
            self._reader_script = None

        if self._tracer_script is not None:
            try:
                self._tracer_script.unload()
            except:
                pass
            self._tracer_script = None

    def start(
        self, device: frida.core.Device, session: frida.core.Session, runtime: str, ui: InstructionTracerUI
    ) -> None:
        def on_message(message, data) -> None:
            self._reactor.schedule(lambda: self._on_message(message, data))

        self._ui = ui

        agent_source = (Path(__file__).parent / "itracer_agent.js").read_text(encoding="utf-8")

        try:
            tracer_script = session.create_script(name="itracer", source=agent_source, runtime=runtime)
            self._tracer_script = tracer_script
            self._ui._on_script_created(tracer_script)
            tracer_script.on("message", on_message)
            tracer_script.load()

            tracer_api = tracer_script.exports_sync

            outpath = ui.get_trace_output_path()
            if outpath is None:
                outpath = ui.prompt_for_trace_output_path(suggested_name=tracer_api.query_program_name() + ".itrace")
                if outpath is None:
                    ui.on_trace_stopped("Missing output path")
                    return

            self._outfile = open(outpath, "wb")
            self._outfile.write(self.FILE_MAGIC)

            strategy = ui.get_trace_strategy()
            if strategy is None:
                strategy = ui.prompt_for_trace_strategy(threads=tracer_api.list_threads())
                if strategy is None:
                    ui.on_trace_stopped("Missing strategy")
                    return

            buffer_location = tracer_api.create_buffer()

            try:
                system_session = device.attach(0)

                reader_script = system_session.create_script(name="itracer", source=agent_source, runtime=runtime)
                self._reader_script = reader_script
                self._ui._on_script_created(reader_script)
                reader_script.on("message", on_message)
                reader_script.load()

                reader_script.exports_sync.open_buffer(buffer_location)
            except:
                if self._reader_script is not None:
                    self._reader_script.unload()
                    self._reader_script = None
                reader_script = None

            if reader_script is not None:
                reader_api = reader_script.exports_sync
            else:
                reader_api = tracer_script.exports_sync
            self._reader_api = reader_api
            reader_api.launch_buffer_reader()

            tracer_script.exports_sync.launch_trace_session(strategy)

            ui.on_trace_started()
        except RPCException as e:
            ui.on_trace_stopped(f"Unable to start: {e.args[0]}")
        except Exception as e:
            ui.on_trace_stopped(str(e))
        except KeyboardInterrupt:
            ui.on_trace_stopped()

    def _on_message(self, message, data) -> None:
        handled = False

        if message["type"] == "send":
            try:
                payload = message["payload"]
                mtype = payload["type"]
                params = (mtype, payload, data)
            except:
                params = None
            if params is not None:
                handled = self._try_handle_message(*params)

        if not handled:
            print(message)

    def _try_handle_message(self, mtype, message, data) -> bool:
        if not mtype.startswith("itrace:"):
            return False

        if mtype == "itrace:chunk":
            self._write_chunk(data)
        else:
            self._write_message(message, data)

            if mtype == "itrace:compile":
                self._total_blocks += 1

        self._update_progress()

        if mtype == "itrace:end":
            self._ui.on_trace_stopped()

        return True

    def _update_progress(self) -> None:
        self._ui.on_trace_progress(self._total_blocks, self._outfile.tell())

    def _write_message(self, message, data) -> None:
        f = self._outfile

        raw_message = json.dumps(message).encode("utf-8")
        f.write(struct.pack(">II", RecordType.MESSAGE, len(raw_message)))
        f.write(raw_message)

        data_size = len(data) if data is not None else 0
        f.write(struct.pack(">I", data_size))
        if data_size != 0:
            f.write(data)

        f.flush()

    def _write_chunk(self, chunk) -> None:
        f = self._outfile
        f.write(struct.pack(">II", RecordType.CHUNK, len(chunk)))
        f.write(chunk)
        f.flush()


class RecordType:
    MESSAGE = 1
    CHUNK = 2


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
