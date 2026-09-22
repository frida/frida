import argparse
from typing import List

import frida

from frida_tools.application import ConsoleApplication, expand_target, infer_target


class KillApplication(ConsoleApplication):
    def _usage(self) -> str:
        return "%(prog)s [options] process"

    def _add_options(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("process", help="process name or pid")

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        process = expand_target(infer_target(options.process))
        if process[0] == "file":
            parser.error("process name or pid must be specified")

        self._process = process[1]

    def _start(self) -> None:
        try:
            assert self._device is not None
            self._device.kill(self._process)
        except frida.ProcessNotFoundError:
            self._update_status(f"unable to find process: {self._process}")
            self._exit(1)
        self._exit(0)


def main() -> None:
    app = KillApplication()
    app.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
