import argparse
import codecs
import os
import sys
from typing import Any, List

from colorama import Fore, Style

from frida_tools.application import ConsoleApplication


def main() -> None:
    app = RmApplication()
    app.run()


class RmApplication(ConsoleApplication):
    def _add_options(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("files", help="files to remove", nargs="+")
        parser.add_argument("-f", "--force", help="ignore nonexistent files", action="store_true")
        parser.add_argument(
            "-r", "--recursive", help="remove directories and their contents recursively", action="store_true"
        )

    def _usage(self) -> str:
        return "%(prog)s [options] FILE..."

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        self._paths = options.files
        self._flags = []
        if options.force:
            self._flags.append("force")
        if options.recursive:
            self._flags.append("recursive")

    def _needs_target(self) -> bool:
        return False

    def _start(self) -> None:
        try:
            self._attach(self._pick_worker_pid())

            data_dir = os.path.dirname(__file__)
            with codecs.open(os.path.join(data_dir, "fs_agent.js"), "r", "utf-8") as f:
                source = f.read()

            def on_message(message: Any, data: Any) -> None:
                self._reactor.schedule(lambda: self._on_message(message, data))

            assert self._session is not None
            script = self._session.create_script(name="pull", source=source)
            script.on("message", on_message)
            self._on_script_created(script)
            script.load()

            errors = script.exports_sync.rm(self._paths, self._flags)

            for message in errors:
                self._print(Fore.RED + Style.BRIGHT + message + Style.RESET_ALL, file=sys.stderr)

            status = 0 if len(errors) == 0 else 1
            self._exit(status)
        except Exception as e:
            self._update_status(str(e))
            self._exit(1)
            return

    def _on_message(self, message: Any, data: Any) -> None:
        print(message)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
