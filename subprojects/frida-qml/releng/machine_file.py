from configparser import ConfigParser
from pathlib import Path
from typing import Dict, List, Sequence, Union


def load(mfile: Path) -> Dict[str, Union[str, List[str]]]:
    config = ConfigParser()
    config.read(mfile)

    hidden_constants = {
        "true": True,
        "false": False,
    }

    items = {}
    if config.has_section("constants"):
        for name, raw_value in config.items("constants"):
            items[name] = eval(raw_value, hidden_constants, items)

    for section_name, section in config.items():
        if section_name in ("DEFAULT", "constants"):
            continue
        for name, raw_value in section.items():
            value = eval(raw_value, hidden_constants, items)
            if section_name == "binaries" and isinstance(value, str):
                value = [value]
            items[name] = value

    if len(items) == 0:
        return None

    return items


def bool_to_meson(b: bool) -> str:
    return "true" if b else "false"


def strv_to_meson(strv: Sequence[str]) -> str:
    return "[" + ", ".join(map(str_to_meson, strv)) + "]"


def str_to_meson(s: str) -> str:
    return "'" + s + "'"
