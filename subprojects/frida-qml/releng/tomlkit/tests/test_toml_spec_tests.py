import json
import os
import re

import pytest
import yaml

from tomlkit import parse
from tomlkit._compat import decode
from tomlkit._utils import parse_rfc3339
from tomlkit.exceptions import TOMLKitError


IGNORED_TESTS = []
# The following tests trigger a RecursionError
IGNORED_TESTS += ["qa-array-inline-nested-1000", "qa-table-inline-nested-1000"]
# The following tests don't work due to time microseconds precision of the tests
IGNORED_TESTS += ["spec-date-time-6", "spec-date-time-local-2", "spec-time-2"]
# The following tests don't work due to nan always comparing to False
IGNORED_TESTS += ["spec-float-13", "spec-float-14", "spec-float-15"]
# The following tests don't work due to issues with th epyyaml library
IGNORED_TESTS += ["spec-key-value-pair-9"]
SPEC_TEST_DIR = os.path.join(os.path.dirname(__file__), "toml-spec-tests")
VALID_TESTS = sorted(
    os.path.basename(f).rsplit(".", 1)[0]
    for f in os.listdir(os.path.join(SPEC_TEST_DIR, "values"))
    if os.path.basename(f).rsplit(".", 1)[0] not in IGNORED_TESTS
)
ERROR_TESTS = sorted(
    os.path.basename(f).rsplit(".", 1)[0]
    for f in os.listdir(os.path.join(SPEC_TEST_DIR, "errors"))
    if os.path.basename(f).rsplit(".", 1)[0] not in IGNORED_TESTS
)


def to_bool(s):
    assert s in ["true", "false"]

    return s == "true"


stypes = {
    "string": str,
    "bool": to_bool,
    "integer": int,
    "float": float,
    "datetime": parse_rfc3339,
    "datetime-local": parse_rfc3339,
    "date": parse_rfc3339,
    "time": parse_rfc3339,
}

loader = yaml.SafeLoader
loader.add_implicit_resolver(
    "tag:yaml.org,2002:float",
    re.compile(
        """^(?:
     [-+]?(?:[0-9][0-9_]*)\\.[0-9_]*(?:[eE][-+]?[0-9]+)?
    |[-+]?(?:[0-9][0-9_]*)(?:[eE][-+]?[0-9]+)
    |\\.[0-9_]+(?:[eE][-+][0-9]+)?
    |[-+]?[0-9][0-9_]*(?::[0-5]?[0-9])+\\.[0-9_]*
    |[-+]?\\.(?:inf|Inf|INF)
    |\\.(?:nan|NaN|NAN))$""",
        re.X,
    ),
    list("-+0123456789."),
)


def untag(value):
    if isinstance(value, list):
        return [untag(i) for i in value]
    elif "type" in value and "value" in value and len(value) == 2:
        if value["type"] in stypes:
            val = decode(value["value"])

            return stypes[value["type"]](val)
        elif value["type"] == "array":
            return [untag(i) for i in value["value"]]
        else:
            raise Exception(f'Unsupported type {value["type"]}')
    else:
        return {k: untag(v) for k, v in value.items()}


@pytest.mark.parametrize("test", VALID_TESTS)
def test_valid_decode(test):
    toml_file = os.path.join(SPEC_TEST_DIR, "values", test + ".toml")
    yaml_file = os.path.join(SPEC_TEST_DIR, "values", test + ".yaml")
    with open(toml_file, encoding="utf-8") as f:
        toml_content = f.read()
        toml_val = parse(toml_content)

    if os.path.exists(yaml_file):
        with open(yaml_file, encoding="utf-8") as f:
            yaml_val = yaml.load(f.read(), Loader=loader)
    else:
        with open(
            os.path.join(SPEC_TEST_DIR, "values", test + ".json"), encoding="utf-8"
        ) as f:
            yaml_val = untag(json.loads(f.read()))

    assert toml_val == yaml_val
    assert toml_val.as_string() == toml_content


@pytest.mark.parametrize("test", ERROR_TESTS)
def test_invalid_decode(test):
    toml_file = os.path.join(SPEC_TEST_DIR, "errors", test + ".toml")
    with pytest.raises(TOMLKitError), open(toml_file, encoding="utf-8") as f:
        parse(f.read())
