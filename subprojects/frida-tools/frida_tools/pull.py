import argparse
import codecs
import os
import sys
import time
import typing
from threading import Thread
from typing import Any, AnyStr, List, Mapping, Optional

import frida
from colorama import Fore, Style

from frida_tools.application import ConsoleApplication
from frida_tools.stream_controller import StreamController
from frida_tools.units import bytes_to_megabytes


def main() -> None:
    app = PullApplication()
    app.run()


class PullApplication(ConsoleApplication):
    def _add_options(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("files", help="remote files to pull", nargs="+")

    def _usage(self) -> str:
        return "%(prog)s [options] REMOTE... LOCAL"

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        paths = options.files
        if len(paths) == 1:
            self._remote_paths = paths
            self._local_paths = [os.path.join(os.getcwd(), basename_of_unknown_path(paths[0]))]
        elif len(paths) == 2:
            remote, local = paths
            self._remote_paths = [remote]
            if os.path.isdir(local):
                self._local_paths = [os.path.join(local, basename_of_unknown_path(remote))]
            else:
                self._local_paths = [local]
        else:
            self._remote_paths = paths[:-1]
            local_dir = paths[-1]
            local_filenames = map(basename_of_unknown_path, self._remote_paths)
            self._local_paths = [os.path.join(local_dir, filename) for filename in local_filenames]

        self._script: Optional[frida.core.Script] = None
        self._stream_controller: Optional[StreamController] = None
        self._total_bytes = 0
        self._time_started: Optional[float] = None
        self._failed_paths = []

    def _needs_target(self) -> bool:
        return False

    def _start(self) -> None:
        try:
            self._attach(self._pick_worker_pid())

            data_dir = os.path.dirname(__file__)
            with codecs.open(os.path.join(data_dir, "fs_agent.js"), "r", "utf-8") as f:
                source = f.read()

            def on_message(message: Mapping[Any, Any], data: Any) -> None:
                self._reactor.schedule(lambda: self._on_message(message, data))

            assert self._session is not None
            script = self._session.create_script(name="pull", source=source)
            self._script = script
            script.on("message", on_message)
            self._on_script_created(script)
            script.load()

            self._stream_controller = StreamController(
                self._post_stream_stanza,
                self._on_incoming_stream_request,
                on_stats_updated=self._on_stream_stats_updated,
            )

            worker = Thread(target=self._perform_pull)
            worker.start()
        except Exception as e:
            self._update_status(f"Failed to pull: {e}")
            self._exit(1)
            return

    def _stop(self) -> None:
        if self._stream_controller is not None:
            self._stream_controller.dispose()

    def _perform_pull(self) -> None:
        error = None
        try:
            assert self._script is not None
            self._script.exports_sync.pull(self._remote_paths)
        except Exception as e:
            error = e

        self._reactor.schedule(lambda: self._on_pull_finished(error))

    def _on_pull_finished(self, error: Optional[Exception]) -> None:
        for path, state in self._failed_paths:
            if state == "partial":
                try:
                    os.unlink(path)
                except:
                    pass

        if error is None:
            self._render_summary_ui()
        else:
            self._print_error(str(error))

        success = len(self._failed_paths) == 0 and error is None
        status = 0 if success else 1
        self._exit(status)

    def _render_progress_ui(self) -> None:
        assert self._stream_controller is not None
        megabytes_received = bytes_to_megabytes(self._stream_controller.bytes_received)
        total_megabytes = bytes_to_megabytes(self._total_bytes)
        if total_megabytes != 0 and megabytes_received <= total_megabytes:
            self._update_status(f"Pulled {megabytes_received:.1f} out of {total_megabytes:.1f} MB")
        else:
            self._update_status(f"Pulled {megabytes_received:.1f} MB")

    def _render_summary_ui(self) -> None:
        assert self._time_started is not None
        duration = time.time() - self._time_started

        if len(self._remote_paths) == 1:
            prefix = f"{self._remote_paths[0]}: "
        else:
            prefix = ""

        assert self._stream_controller is not None
        sc = self._stream_controller
        bytes_received = sc.bytes_received
        megabytes_per_second = bytes_to_megabytes(bytes_received) / duration

        self._update_status(
            "{}{} file{} pulled. {:.1f} MB/s ({} bytes in {:.3f}s)".format(
                prefix,
                sc.streams_opened,
                "s" if sc.streams_opened != 1 else "",
                megabytes_per_second,
                bytes_received,
                duration,
            )
        )

    def _on_message(self, message: Mapping[Any, Any], data: Any) -> None:
        handled = False

        if message["type"] == "send":
            payload = message["payload"]
            ptype = payload["type"]
            if ptype == "stream":
                stanza = payload["payload"]
                assert self._stream_controller is not None
                self._stream_controller.receive(stanza, data)
                handled = True
            elif ptype == "pull:status":
                self._total_bytes = payload["total"]
                self._time_started = time.time()
                self._render_progress_ui()
                handled = True
            elif ptype == "pull:io-error":
                index = payload["index"]
                self._on_io_error(self._remote_paths[index], self._local_paths[index], payload["error"])
                handled = True

        if not handled:
            self._print(message)

    def _on_io_error(self, remote_path, local_path, error) -> None:
        self._print_error(f"{remote_path}: {error}")
        self._failed_paths.append((local_path, "partial"))

    def _post_stream_stanza(self, stanza, data: Optional[AnyStr] = None) -> None:
        self._script.post({"type": "stream", "payload": stanza}, data=data)

    def _on_incoming_stream_request(self, label: str, details) -> typing.BinaryIO:
        local_path = self._local_paths[int(label)]
        try:
            return open(local_path, "wb")
        except Exception as e:
            self._print_error(str(e))
            self._failed_paths.append((local_path, "unopened"))
            raise

    def _on_stream_stats_updated(self) -> None:
        self._render_progress_ui()

    def _print_error(self, message: str) -> None:
        self._print(Fore.RED + Style.BRIGHT + message + Style.RESET_ALL, file=sys.stderr)


def basename_of_unknown_path(path: str) -> str:
    return path.replace("\\", "/").rsplit("/", 1)[-1]


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
