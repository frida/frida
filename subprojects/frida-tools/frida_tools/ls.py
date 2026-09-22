import argparse
import codecs
import os
from datetime import datetime, timezone
from operator import itemgetter
from typing import Any, List

from colorama import Fore, Style

from frida_tools.application import ConsoleApplication

STYLE_DIR = Fore.BLUE + Style.BRIGHT
STYLE_EXECUTABLE = Fore.GREEN + Style.BRIGHT
STYLE_LINK = Fore.CYAN + Style.BRIGHT
STYLE_ERROR = Fore.RED + Style.BRIGHT


def main() -> None:
    app = LsApplication()
    app.run()


class LsApplication(ConsoleApplication):
    def _add_options(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("files", help="files to list information about", nargs="*")

    def _usage(self) -> str:
        return "%(prog)s [options] [FILE]..."

    def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
        self._files = options.files

    def _needs_target(self) -> bool:
        return False

    def _start(self) -> None:
        try:
            self._attach(self._pick_worker_pid())

            data_dir = os.path.dirname(__file__)
            with codecs.open(os.path.join(data_dir, "fs_agent.js"), "r", "utf-8") as f:
                source = f.read()

            def on_message(message: Any, data: Any) -> None:
                print(message)

            assert self._session is not None
            script = self._session.create_script(name="ls", source=source)
            script.on("message", on_message)
            self._on_script_created(script)
            script.load()

            groups = script.exports_sync.ls(self._files)
        except Exception as e:
            self._update_status(f"Failed to retrieve listing: {e}")
            self._exit(1)
            return

        exit_status = 0
        for i, group in enumerate(sorted(groups, key=lambda g: g["path"])):
            path = group["path"]
            if path != "" and len(groups) > 1:
                if i > 0:
                    self._print("")
                self._print(path + ":")

            for path, message in group["errors"]:
                self._print(STYLE_ERROR + message + Style.RESET_ALL)
                exit_status = 2

            rows = []
            for name, target, type, access, nlink, owner, group, size, raw_mtime in group["entries"]:
                mtime = datetime.fromtimestamp(raw_mtime / 1000.0, tz=timezone.utc)
                rows.append((type + access, str(nlink), owner, group, str(size), mtime.strftime("%c"), name, target))

            if len(rows) == 0:
                break

            widths = []
            for column_index in range(len(rows[0]) - 2):
                width = max(map(lambda row: len(row[column_index]), rows))
                widths.append(width)

            adjustments = [
                "",
                ">",
                "<",
                "<",
                ">",
                "<",
            ]
            col_formats = []
            for i, width in enumerate(widths):
                adj = adjustments[i]
                if adj != "":
                    fmt = "{:" + adj + str(width) + "}"
                else:
                    fmt = "{}"
                col_formats.append(fmt)
            row_description = " ".join(col_formats)

            for row in sorted(rows, key=itemgetter(6)):
                meta_fields = row_description.format(*row[:-2])

                name, target = row[6:8]
                ftype_and_perms = row[0]
                ftype = ftype_and_perms[0]
                fperms = ftype_and_perms[1:]
                name = format_name(name, ftype, fperms, target)

                self._print(meta_fields + " " + name)

        self._exit(exit_status)


def format_name(name: str, ftype: str, fperms: str, target) -> str:
    if ftype == "l":
        target_path, target_details = target
        if target_details is not None:
            target_type, target_perms = target_details
            target_summary = format_name(target_path, target_type, target_perms, None)
        else:
            target_summary = STYLE_ERROR + target_path + Style.RESET_ALL
        return STYLE_LINK + name + Style.RESET_ALL + " -> " + target_summary

    if ftype == "d":
        return STYLE_DIR + name + Style.RESET_ALL

    if "x" in fperms:
        return STYLE_EXECUTABLE + name + Style.RESET_ALL

    return name


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
