from dataclasses import dataclass
import os
import re
from typing import Dict, List


RELENG_DIR = os.path.abspath(os.path.dirname(__file__))

CONFIG_KEY_VALUE_PATTERN = re.compile(r"^([a-z]\w+) = (.*?)(?<!\\)$", re.MULTILINE | re.DOTALL)
CONFIG_VARIABLE_REF_PATTERN = re.compile(r"\$\((\w+)\)")


@dataclass
class PackageSpec:
    version: str
    url: str
    hash: str
    recipe: str
    patches: List[str]
    deps: List[str]
    deps_for_build: List[str]
    options: List[str]


@dataclass
class DependencyParameters:
    toolchain_version: str
    sdk_version: str
    bootstrap_version: str
    packages: Dict[str, PackageSpec]

    def get_package_spec(self, name: str) -> PackageSpec:
        return self.packages[name.replace("-", "_")]


def read_dependency_parameters(host_defines: Dict[str, str]) -> DependencyParameters:
    raw_params = host_defines.copy()
    with open(os.path.join(RELENG_DIR, "deps.mk"), encoding='utf-8') as f:
        for match in CONFIG_KEY_VALUE_PATTERN.finditer(f.read()):
            key, value = match.group(1, 2)
            value = value \
                    .replace("\\\n", " ") \
                    .replace("\t", " ") \
                    .replace("$(NULL)", "") \
                    .strip()
            while "  " in value:
                value = value.replace("  ", " ")
            raw_params[key] = value

    packages = {}
    for key in [k for k in raw_params.keys() if k.endswith("_recipe")]:
        name = key[:-7]
        packages[name] = PackageSpec(
                parse_string_value(raw_params[name + "_version"], raw_params),
                parse_string_value(raw_params[name + "_url"], raw_params),
                parse_string_value(raw_params[name + "_hash"], raw_params),
                parse_string_value(raw_params[name + "_recipe"], raw_params),
                parse_array_value(raw_params[name + "_patches"], raw_params),
                parse_array_value(raw_params[name + "_deps"], raw_params),
                parse_array_value(raw_params[name + "_deps_for_build"], raw_params),
                parse_array_value(raw_params[name + "_options"], raw_params))

    return DependencyParameters(
            raw_params["frida_toolchain_version"],
            raw_params["frida_sdk_version"],
            raw_params["frida_bootstrap_version"],
            packages)


def parse_string_value(v: str, raw_params: Dict[str, str]) -> str:
    return CONFIG_VARIABLE_REF_PATTERN.sub(lambda match: raw_params[match.group(1)], v)


def parse_array_value(v: str, raw_params: Dict[str, str]) -> List[str]:
    v = parse_string_value(v, raw_params)
    if v == "":
        return []
    return v.split(" ")
