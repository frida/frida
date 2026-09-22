import re
from pathlib import Path

from .generatorbase import GeneratorBase
from .model import (
    ReferenceManual,
    Function,
    Object,
    PosArg,
    VarArgs,
    Kwarg,
)

import typing as T


class ManPage:
    def __init__(self, path: Path):
        self.path = path
        self.text = ""

    def reset_font(self) -> None:
        self.text += ".P\n"

    def title(self, name: str, section: int) -> None:
        import datetime

        date = datetime.date.today()
        self.reset_font()
        self.text += f'.TH "{name}" "{section}" "{date}"\n'

    def section(self, name: str) -> None:
        self.reset_font()
        self.text += f".SH {name}\n"

    def subsection(self, name: str) -> None:
        self.reset_font()
        self.text += f".SS {name}\n"

    def par(self, text: str) -> None:
        self.reset_font()
        self.text += f"{text}\n"

    def indent(self, amount: int = 4) -> None:
        self.text += f".RS {amount}\n"

    def unindent(self) -> None:
        self.text += ".RE\n"

    def br(self) -> None:
        self.text += ".br\n"

    def nl(self) -> None:
        self.text += "\n"

    def line(self, text: str) -> None:
        if text and text[0] in [".", "'"]:
            self.text += "\\"
        self.text += f"{text}\n"

    def inline(self, text: str) -> None:
        self.text += f"{text}"

    def write(self) -> None:
        self.path.write_text(self.text, encoding="utf-8")

    @staticmethod
    def bold(text: str) -> str:
        return f"\\fB{text}\\fR"

    @staticmethod
    def italic(text: str) -> str:
        return f"\\fI{text}\\fR"


class GeneratorMan(GeneratorBase):
    def __init__(
        self, manual: ReferenceManual, out: Path, enable_modules: bool
    ) -> None:
        super().__init__(manual)
        self.out = out
        self.enable_modules = enable_modules
        self.links: T.List[str] = []

    def generate_description(self, page: ManPage, desc: str) -> None:
        def italicise(match: T.Match[str]) -> str:
            v = match.group(1)
            if v[0] == "@":
                v = v[1:]

            return ManPage.italic(v)

        desc = re.sub(re.compile(r"\[\[(.*?)\]\]", re.DOTALL), italicise, desc)

        def linkify(match: T.Match[str]) -> str:
            replacement = ManPage.italic(match.group(1))

            if match.group(2)[0] != "#":
                if match.group(2) in self.links:
                    num = self.links.index(match.group(2))
                else:
                    self.links.append(match.group(2))
                    num = len(self.links)

                replacement += f"[{num}]"

            return replacement

        desc = re.sub(re.compile(r"\[(.*?)\]\((.*?)\)", re.DOTALL), linkify, desc)

        def bold(match: T.Match[str]) -> str:
            return ManPage.bold(match.group(1))

        desc = re.sub(re.compile(r"\*(.*?)\*"), bold, desc)

        isCode = False
        for chunk in desc.split("```"):
            if isCode:
                page.indent()
                lines = chunk.strip().split("\n")
                if lines[0] == "meson":
                    lines = lines[1:]

                for line in lines:
                    page.line(line)
                    page.br()
                page.unindent()
            else:
                inList = False
                for line in chunk.strip().split("\n"):
                    if len(line) == 0:
                        page.nl()
                        if inList:
                            page.nl()
                            inList = False
                    elif line[0:2] in ["- ", "* "]:
                        if inList:
                            page.nl()
                            page.br()
                        else:
                            inList = True

                        page.inline(line.strip() + " ")
                    elif inList and line[0] == " ":
                        page.inline(line.strip() + " ")
                    else:
                        inList = False
                        page.line(line)

                if inList:
                    page.nl()

            isCode = not isCode

    def function_name(self, f: Function, o: Object = None) -> str:
        name = ""
        if o is not None:
            name += f"{o.name}."

        name += f.name
        return name

    def generate_function_signature(
        self, page: ManPage, f: Function, o: Object = None
    ) -> None:
        args = []

        if f.posargs:
            args += [arg.name for arg in f.posargs]

        if f.varargs:
            args += [f.varargs.name + "..."]

        if f.optargs:
            args += [f"[{arg.name}]" for arg in f.optargs]

        for kwarg in self.sorted_and_filtered(list(f.kwargs.values())):
            kw = kwarg.name + ":"
            if kwarg.default:
                kw += " " + ManPage.bold(kwarg.default)
            args += [kw]

        ret = ManPage.italic(f.returns.raw) + " "

        prefix = f"{ret}{self.function_name(f, o)}("
        sig = ", ".join(args)
        suffix = ")"

        if len(prefix) + len(sig) + len(suffix) > 70:
            page.line(prefix)
            page.br()
            page.indent()
            for arg in args:
                page.line(arg + ",")
                page.br()
            page.unindent()
            page.line(suffix)
        else:
            page.line(prefix + sig + suffix)

    def base_info(
        self, x: T.Union[PosArg, VarArgs, Kwarg, Function, Object]
    ) -> T.List[str]:
        info = []
        if x.deprecated:
            info += [ManPage.bold("deprecated") + f" since {x.deprecated}"]
        if x.since:
            info += [f"since {x.since}"]

        return info

    def generate_function_arg(
        self,
        page: ManPage,
        arg: T.Union[PosArg, VarArgs, Kwarg],
        isOptarg: bool = False,
    ) -> None:
        required = (
            arg.required
            if isinstance(arg, Kwarg)
            else not isOptarg and not isinstance(arg, VarArgs)
        )

        page.line(ManPage.bold(arg.name))

        info = [ManPage.italic(arg.type.raw)]

        if required:
            info += [ManPage.bold("required")]
        if isinstance(arg, (PosArg, Kwarg)) and arg.default:
            info += [f"default: {arg.default}"]
        if isinstance(arg, VarArgs):
            mn = 0 if arg.min_varargs < 0 else arg.min_varargs
            mx = "N" if arg.max_varargs < 0 else arg.max_varargs
            info += [f"{mn}...{mx} times"]

        info += self.base_info(arg)

        page.line(", ".join(info))

        page.br()
        page.indent(2)
        self.generate_description(page, arg.description.strip())
        page.unindent()
        page.nl()

    def generate_function_argument_section(
        self,
        page: ManPage,
        name: str,
        args: T.Sequence[T.Union[PosArg, VarArgs, Kwarg]],
        isOptarg: bool = False,
    ) -> None:
        if not args:
            return

        page.line(ManPage.bold(name))
        page.indent()
        for arg in args:
            self.generate_function_arg(page, arg, isOptarg)
        page.unindent()

    def generate_sub_sub_section(
        self, page: ManPage, name: str, text: T.List[str], process: bool = True
    ) -> None:
        page.line(ManPage.bold(name))
        page.indent()
        if process:
            for line in text:
                self.generate_description(page, line.strip())
        else:
            page.line("\n\n".join([line.strip() for line in text]))
        page.unindent()

    def generate_function(self, page: ManPage, f: Function, obj: Object = None) -> None:
        page.subsection(self.function_name(f, obj) + "()")
        page.indent(0)

        page.line(ManPage.bold("SYNOPSIS"))
        page.indent()
        self.generate_function_signature(page, f, obj)

        info = self.base_info(f)
        if info:
            page.nl()
            page.line(", ".join(info))
        page.unindent()
        page.nl()

        self.generate_sub_sub_section(page, "DESCRIPTION", [f.description])
        page.nl()

        self.generate_function_argument_section(page, "POSARGS", f.posargs)
        if f.varargs:
            self.generate_function_argument_section(page, "VARARGS", [f.varargs])
        self.generate_function_argument_section(page, "OPTARGS", f.optargs, True)
        self.generate_function_argument_section(
            page, "KWARGS", self.sorted_and_filtered(list(f.kwargs.values()))
        )

        if f.notes:
            self.generate_sub_sub_section(page, "NOTES", f.notes)
        if f.warnings:
            self.generate_sub_sub_section(page, "WARNINGS", f.warnings)
        if f.example:
            self.generate_sub_sub_section(page, "EXAMPLE", [f.example])

        page.unindent()

    def generate_object(self, page: ManPage, obj: Object) -> None:
        page.subsection(obj.name)
        page.indent(2)

        info = self.base_info(obj)
        if info:
            page.line(", ".join(info))
            page.br()

        if obj.extends:
            page.line(ManPage.bold("extends: ") + obj.extends)
            page.br()

        ret = [x.name for x in self.sorted_and_filtered(obj.returned_by)]
        if ret:
            page.line(ManPage.bold("returned_by: ") + ", ".join(ret))
            page.br()

        ext = [x.name for x in self.sorted_and_filtered(obj.extended_by)]
        if ext:
            page.line(ManPage.bold("extended_by: ") + ", ".join(ext))
            page.br()

        page.nl()

        self.generate_description(page, obj.description.strip())
        page.nl()

        if obj.notes:
            self.generate_sub_sub_section(page, "NOTES", obj.notes)
        if obj.warnings:
            self.generate_sub_sub_section(page, "WARNINGS", obj.warnings)
        if obj.example:
            self.generate_sub_sub_section(page, "EXAMPLE", [obj.example])

        page.unindent()

    def generate(self) -> None:
        page = ManPage(self.out)

        page.title("meson-reference", 3)

        page.section("NAME")
        page.par(
            f"meson-reference v{self._extract_meson_version()}"
            + " - a reference for meson functions and objects"
        )

        page.section("DESCRIPTION")
        self.generate_description(
            page,
            """This manual is divided into two sections, *FUNCTIONS* and *OBJECTS*.  *FUNCTIONS* contains a reference for all meson functions and methods.  Methods are denoted by [[object_name]].[[method_name]]().  *OBJECTS* contains additional information about each object.""",
        )

        page.section("FUNCTIONS")
        for f in self.sorted_and_filtered(self.functions):
            self.generate_function(page, f)

        for obj in self.sorted_and_filtered(self.objects):
            for f in self.sorted_and_filtered(obj.methods):
                self.generate_function(page, f, obj)

        page.section("OBJECTS")
        for obj in self.sorted_and_filtered(self.objects):
            self.generate_object(page, obj)

        page.section("SEE ALSO")
        for i in range(len(self.links)):
            link = self.links[i]
            page.line(f"[{i + 1}] {link}")
            page.br()

        page.write()
