import abc
import codecs
import json
import os
from typing import TYPE_CHECKING, Optional, Sequence

if TYPE_CHECKING:
    import frida_tools.repl


class Magic(abc.ABC):
    @property
    def description(self) -> str:
        return "no description"

    @abc.abstractproperty
    def required_args_count(self) -> int:
        pass

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> Optional[bool]:
        pass


class Resume(Magic):
    @property
    def description(self) -> str:
        return "resume execution of the spawned process"

    @property
    def required_args_count(self) -> int:
        return 0

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        repl._reactor.schedule(lambda: repl._resume())


class Load(Magic):
    @property
    def description(self) -> str:
        return "Load an additional script and reload the current REPL state"

    @property
    def required_args_count(self) -> int:
        return 1

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        try:
            proceed = repl._get_confirmation(
                "Are you sure you want to load a new script and discard all current state?"
            )
            if not proceed:
                repl._print("Discarding load command")
                return

            repl._user_scripts.append(args[0])
            repl._perform_on_reactor_thread(lambda: repl._load_script())
        except Exception as e:
            repl._print(f"Failed to load script: {e}")


class Reload(Magic):
    @property
    def description(self) -> str:
        return "reload (i.e. rerun) the script that was given as an argument to the REPL"

    @property
    def required_args_count(self) -> int:
        return 0

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> bool:
        try:
            repl._perform_on_reactor_thread(lambda: repl._load_script())
            return True
        except Exception as e:
            repl._print(f"Failed to load script: {e}")
            return False


class Unload(Magic):
    @property
    def required_args_count(self) -> int:
        return 0

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        repl._unload_script()


class Autoperform(Magic):
    @property
    def description(self) -> str:
        return (
            "receive on/off as first and only argument, when switched on will wrap any REPL code with Java.performNow()"
        )

    @property
    def required_args_count(self) -> int:
        return 1

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        repl._autoperform_command(args[0])


class Autoreload(Magic):
    _VALID_ARGUMENTS = ("on", "off")

    @property
    def description(self) -> str:
        return "disable or enable auto reloading of script files"

    @property
    def required_args_count(self) -> int:
        return 1

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        if args[0] not in self._VALID_ARGUMENTS:
            raise ValueError("Autoreload command only receive on or off as an argument")

        required_state = args[0] == "on"
        if required_state == repl._autoreload:
            repl._print("Autoreloading is already in the desired state")
            return

        if required_state:
            repl._monitor_all()
        else:
            repl._demonitor_all()
        repl._autoreload = required_state


class Exec(Magic):
    @property
    def description(self) -> str:
        return "execute the given file path in the context of the currently loaded scripts"

    @property
    def required_args_count(self) -> int:
        return 1

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        if not os.path.exists(args[0]):
            repl._print("Can't read the given file because it does not exist")
            return

        try:
            with codecs.open(args[0], "rb", "utf-8") as f:
                if not repl._exec_and_print(repl._evaluate_expression, f.read()):
                    repl._errors += 1
        except PermissionError:
            repl._print("Can't read the given file because of a permission error")


class Time(Magic):
    @property
    def description(self) -> str:
        return "measure the execution time of the given expression and print it to the screen"

    @property
    def required_args_count(self) -> int:
        return -2

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        repl._exec_and_print(
            repl._evaluate_expression,
            """
            (() => {{
                const _startTime = Date.now();
                const _result = eval({expression});
                const _endTime = Date.now();
                console.log('Time: ' + (_endTime - _startTime) + ' ms.');
                return _result;
            }})();""".format(expression=json.dumps(" ".join(args))),
        )


class Help(Magic):
    @property
    def description(self) -> str:
        return "print a list of available REPL commands"

    @property
    def required_args_count(self) -> int:
        return 0

    def execute(self, repl: "frida_tools.repl.REPLApplication", args: Sequence[str]) -> None:
        repl._print("Available commands: ")
        for name, command in repl._magic_command_args.items():
            if command.required_args_count >= 0:
                required_args = f"({command.required_args_count})"
            else:
                required_args = f"({abs(command.required_args_count) - 1}+)"

            repl._print(f"  %{name}{required_args} - {command.description}")

        repl._print("")
        repl._print("For help with Frida scripting API, check out https://frida.re/docs/")
        repl._print("")
