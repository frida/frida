import argparse
import codecs
import os
import sys
import time
from threading import Event, Thread
from typing import AnyStr, List, MutableMapping, Optional

import frida
from colorama import Fore, Style

from frida_tools.application import ConsoleApplication
from frida_tools.stream_controller import DisposedException, StreamController
from frida_tools.units import bytes_to_megabytes


def main() -> None:
    app = PushApplication()
    app.run()


class PushApplication(ConsoleApplication):
    def _add_options(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("files", help="local files to push", nargs="+")

    def _usage(self) -> str:
        return "%(prog)s [options] LOCAL... REMOTE"

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        paths = options.files
        if len(paths) == 1:
            raise ValueError("missing remote path")
        self._local_paths = paths[:-1]
        self._remote_path = paths[-1]

        self._script: Optional[frida.core.Script] = None
        self._stream_controller: Optional[StreamController] = None
        self._total_bytes = 0
        self._time_started: Optional[float] = None
        self._completed = Event()
        self._transfers: MutableMapping[str, bool] = {}

    def _needs_target(self) -> bool:
        return False

    def _start(self) -> None:
        try:
            self._attach(self._pick_worker_pid())

            data_dir = os.path.dirname(__file__)
            with codecs.open(os.path.join(data_dir, "fs_agent.js"), "r", "utf-8") as f:
                source = f.read()

            def on_message(message, data) -> None:
                self._reactor.schedule(lambda: self._on_message(message, data))

            assert self._session is not None
            script = self._session.create_script(name="push", source=source)
            self._script = script
            script.on("message", on_message)
            self._on_script_created(script)
            script.load()

            self._stream_controller = StreamController(
                self._post_stream_stanza, on_stats_updated=self._on_stream_stats_updated
            )

            worker = Thread(target=self._perform_push)
            worker.start()
        except Exception as e:
            self._update_status(f"Failed to push: {e}")
            self._exit(1)
            return

    def _stop(self) -> None:
        for path in self._local_paths:
            if path not in self._transfers:
                self._complete_transfer(path, success=False)

        if self._stream_controller is not None:
            self._stream_controller.dispose()

    def _perform_push(self) -> None:
        for path in self._local_paths:
            try:
                self._total_bytes += os.path.getsize(path)
            except:
                pass
        self._time_started = time.time()

        for i, path in enumerate(self._local_paths):
            filename = os.path.basename(path)

            try:
                with open(path, "rb") as f:
                    assert self._stream_controller is not None
                    sink = self._stream_controller.open(str(i), {"filename": filename, "target": self._remote_path})
                    while True:
                        chunk = f.read(4 * 1024 * 1024)
                        if len(chunk) == 0:
                            break
                        sink.write(chunk)
                    sink.close()
            except DisposedException:
                break
            except Exception as e:
                self._print_error(str(e))
                self._complete_transfer(path, success=False)

        self._completed.wait()

        self._reactor.schedule(lambda: self._on_push_finished())

    def _on_push_finished(self) -> None:
        successes = self._transfers.values()

        if any(successes):
            self._render_summary_ui()

        status = 0 if all(successes) else 1
        self._exit(status)

    def _render_progress_ui(self) -> None:
        if self._completed.is_set():
            return
        assert self._stream_controller is not None
        megabytes_sent = bytes_to_megabytes(self._stream_controller.bytes_sent)
        total_megabytes = bytes_to_megabytes(self._total_bytes)
        if total_megabytes != 0 and megabytes_sent <= total_megabytes:
            self._update_status(f"Pushed {megabytes_sent:.1f} out of {total_megabytes:.1f} MB")
        else:
            self._update_status(f"Pushed {megabytes_sent:.1f} MB")

    def _render_summary_ui(self) -> None:
        assert self._time_started is not None
        duration = time.time() - self._time_started

        if len(self._local_paths) == 1:
            prefix = f"{self._local_paths[0]}: "
        else:
            prefix = ""

        files_transferred = sum(map(int, self._transfers.values()))

        assert self._stream_controller is not None
        bytes_sent = self._stream_controller.bytes_sent
        megabytes_per_second = bytes_to_megabytes(bytes_sent) / duration

        self._update_status(
            "{}{} file{} pushed. {:.1f} MB/s ({} bytes in {:.3f}s)".format(
                prefix,
                files_transferred,
                "s" if files_transferred != 1 else "",
                megabytes_per_second,
                bytes_sent,
                duration,
            )
        )

    def _on_message(self, message, data) -> None:
        handled = False

        if message["type"] == "send":
            payload = message["payload"]
            ptype = payload["type"]
            if ptype == "stream":
                stanza = payload["payload"]
                self._stream_controller.receive(stanza, data)
                handled = True
            elif ptype == "push:io-success":
                index = payload["index"]
                self._on_io_success(self._local_paths[index])
                handled = True
            elif ptype == "push:io-error":
                index = payload["index"]
                self._on_io_error(self._local_paths[index], payload["error"])
                handled = True

        if not handled:
            self._print(message)

    def _on_io_success(self, local_path: str) -> None:
        self._complete_transfer(local_path, success=True)

    def _on_io_error(self, local_path: str, error) -> None:
        self._print_error(f"{local_path}: {error}")
        self._complete_transfer(local_path, success=False)

    def _complete_transfer(self, local_path: str, success: bool) -> None:
        self._transfers[local_path] = success
        if len(self._transfers) == len(self._local_paths):
            self._completed.set()

    def _post_stream_stanza(self, stanza, data: Optional[AnyStr] = None) -> None:
        self._script.post({"type": "stream", "payload": stanza}, data=data)

    def _on_stream_stats_updated(self) -> None:
        self._render_progress_ui()

    def _print_error(self, message: str) -> None:
        self._print(Fore.RED + Style.BRIGHT + message + Style.RESET_ALL, file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
