from typing import Any, Dict, Union

from colorama import Fore, Style

THEME_COLOR = "#ef6456"

STYLE_FILE = Fore.CYAN + Style.BRIGHT
STYLE_LOCATION = Fore.LIGHTYELLOW_EX
STYLE_ERROR = Fore.RED + Style.BRIGHT
STYLE_WARNING = Fore.YELLOW + Style.BRIGHT
STYLE_CODE = Fore.WHITE + Style.DIM
STYLE_RESET_ALL = Style.RESET_ALL

CATEGORY_STYLE = {
    "warning": STYLE_WARNING,
    "error": STYLE_ERROR,
}


def format_error(error: BaseException) -> str:
    return STYLE_ERROR + str(error) + Style.RESET_ALL


def format_compiling(script_path: str, cwd: str) -> str:
    name = format_filename(script_path, cwd)
    return f"{STYLE_RESET_ALL}Compiling {STYLE_FILE}{name}{STYLE_RESET_ALL}..."


def format_compiled(
    script_path: str, cwd: str, time_started: Union[int, float], time_finished: Union[int, float]
) -> str:
    name = format_filename(script_path, cwd)
    elapsed = int((time_finished - time_started) * 1000.0)
    return f"{STYLE_RESET_ALL}Compiled {STYLE_FILE}{name}{STYLE_RESET_ALL}{STYLE_CODE} ({elapsed} ms){STYLE_RESET_ALL}"


def format_diagnostic(diag: Dict[str, Any], cwd: str) -> str:
    category = diag["category"]
    code = diag["code"]
    text = diag["text"]

    file = diag.get("file", None)
    if file is not None:
        filename = format_filename(file["path"], cwd)
        line = file["line"] + 1
        character = file["character"] + 1

        path_segment = f"{STYLE_FILE}{filename}{STYLE_RESET_ALL}"
        line_segment = f"{STYLE_LOCATION}{line}{STYLE_RESET_ALL}"
        character_segment = f"{STYLE_LOCATION}{character}{STYLE_RESET_ALL}"

        prefix = f"{path_segment}:{line_segment}:{character_segment} - "
    else:
        prefix = ""

    category_style = CATEGORY_STYLE.get(category, STYLE_RESET_ALL)

    return f"{prefix}{category_style}{category}{STYLE_RESET_ALL} {STYLE_CODE}TS{code}{STYLE_RESET_ALL}: {text}"


def format_filename(path: str, cwd: str) -> str:
    if path.startswith(cwd):
        return path[len(cwd) + 1 :]
    return path
