import json

import pytest

from tomlkit import load
from tomlkit import parse
from tomlkit._compat import decode
from tomlkit._utils import parse_rfc3339
from tomlkit.exceptions import TOMLKitError


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
    "date-local": parse_rfc3339,
    "time-local": parse_rfc3339,
}


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


def test_valid_decode(valid_case):
    json_val = untag(json.loads(valid_case["json"]))
    toml_val = parse(valid_case["toml"])

    assert toml_val == json_val
    assert toml_val.as_string() == valid_case["toml"]


def test_invalid_decode(invalid_decode_case):
    with pytest.raises(TOMLKitError):
        parse(invalid_decode_case["toml"])


def test_invalid_encode(invalid_encode_case):
    with pytest.raises((TOMLKitError, UnicodeDecodeError)), open(
        invalid_encode_case, encoding="utf-8"
    ) as f:
        load(f)
