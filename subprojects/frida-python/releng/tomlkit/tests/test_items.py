import copy
import math
import pickle

from datetime import date
from datetime import datetime
from datetime import time
from datetime import timedelta
from datetime import timezone

import pytest

from tests.util import assert_is_ppo
from tests.util import elementary_test
from tomlkit import api
from tomlkit import parse
from tomlkit.exceptions import NonExistentKey
from tomlkit.items import Array
from tomlkit.items import Bool
from tomlkit.items import Comment
from tomlkit.items import InlineTable
from tomlkit.items import Integer
from tomlkit.items import Item
from tomlkit.items import KeyType
from tomlkit.items import Null
from tomlkit.items import SingleKey as Key
from tomlkit.items import String
from tomlkit.items import StringType
from tomlkit.items import Table
from tomlkit.items import Trivia
from tomlkit.items import item
from tomlkit.parser import Parser


@pytest.fixture()
def tz_pst():
    try:
        from datetime import timezone

        return timezone(timedelta(hours=-8), "PST")
    except ImportError:
        from datetime import tzinfo

        class PST(tzinfo):
            def utcoffset(self, dt):
                return timedelta(hours=-8)

            def tzname(self, dt):
                return "PST"

            def dst(self, dt):
                return timedelta(0)

        return PST()


@pytest.fixture()
def tz_utc():
    try:
        from datetime import timezone

        return timezone.utc
    except ImportError:
        from datetime import tzinfo

        class UTC(tzinfo):
            def utcoffset(self, dt):
                return timedelta(hours=0)

            def tzname(self, dt):
                return "UTC"

            def dst(self, dt):
                return timedelta(0)

        return UTC()


def test_item_base_has_no_unwrap():
    trivia = Trivia(indent="\t", comment_ws=" ", comment="For unit test")
    item = Item(trivia)
    try:
        item.unwrap()
    except NotImplementedError:
        pass
    else:
        raise AssertionError("`items.Item` should not implement `unwrap`")


def test_integer_unwrap():
    elementary_test(item(666), int)


def test_float_unwrap():
    elementary_test(item(2.78), float)


def test_false_unwrap():
    elementary_test(item(False), bool)


def test_true_unwrap():
    elementary_test(item(True), bool)


def test_datetime_unwrap():
    dt = datetime.now(tz=timezone.utc)
    elementary_test(item(dt), datetime)


def test_string_unwrap():
    elementary_test(item("hello"), str)


def test_null_unwrap():
    n = Null()
    elementary_test(n, type(None))


def test_aot_unwrap():
    d = item([{"a": "A"}, {"b": "B"}])
    unwrapped = d.unwrap()
    assert_is_ppo(unwrapped, list)
    for du, _ in zip(unwrapped, d):
        assert_is_ppo(du, dict)
        for ku in du:
            vu = du[ku]
            assert_is_ppo(ku, str)
            assert_is_ppo(vu, str)


def test_time_unwrap():
    t = time(3, 8, 14)
    elementary_test(item(t), time)


def test_date_unwrap():
    d = date.today()
    elementary_test(item(d), date)


def test_array_unwrap():
    trivia = Trivia(indent="\t", comment_ws=" ", comment="For unit test")
    i = item(666)
    f = item(2.78)
    b = item(False)
    a = Array([i, f, b], trivia)
    a_unwrapped = a.unwrap()
    assert_is_ppo(a_unwrapped, list)
    assert_is_ppo(a_unwrapped[0], int)
    assert_is_ppo(a_unwrapped[1], float)
    assert_is_ppo(a_unwrapped[2], bool)


def test_abstract_table_unwrap():
    table = item({"foo": "bar"})
    super_table = item({"table": table, "baz": "borg"})

    table_unwrapped = super_table.unwrap()
    sub_table = table_unwrapped["table"]
    assert_is_ppo(table_unwrapped, dict)
    assert_is_ppo(sub_table, dict)
    for ku in sub_table:
        vu = sub_table[ku]
        assert_is_ppo(ku, str)
        assert_is_ppo(vu, str)


def test_key_comparison():
    k = Key("foo")

    assert k == Key("foo")
    assert k == "foo"
    assert k != "bar"
    assert k != 5


def test_items_can_be_appended_to_and_removed_from_a_table():
    string = """[table]
"""

    parser = Parser(string)
    _, table = parser._parse_table()

    assert isinstance(table, Table)
    assert table.as_string() == ""

    table.append(Key("foo"), String(StringType.SLB, "bar", "bar", Trivia(trail="\n")))

    assert table.as_string() == 'foo = "bar"\n'

    table.append(
        Key("baz"),
        Integer(34, Trivia(comment_ws="   ", comment="# Integer", trail=""), "34"),
    )

    assert table.as_string() == 'foo = "bar"\nbaz = 34   # Integer'

    table.remove(Key("baz"))

    assert table.as_string() == 'foo = "bar"\n'

    table.remove(Key("foo"))

    assert table.as_string() == ""

    with pytest.raises(NonExistentKey):
        table.remove(Key("foo"))


def test_items_can_be_appended_to_and_removed_from_an_inline_table():
    string = """table = {}
"""

    parser = Parser(string)
    _, table = parser._parse_item()

    assert isinstance(table, InlineTable)
    assert table.as_string() == "{}"

    table.append(Key("foo"), String(StringType.SLB, "bar", "bar", Trivia(trail="")))

    assert table.as_string() == '{foo = "bar"}'

    table.append(Key("baz"), Integer(34, Trivia(trail=""), "34"))

    assert table.as_string() == '{foo = "bar", baz = 34}'

    table.remove(Key("baz"))

    assert table.as_string() == '{foo = "bar"}'

    table.remove(Key("foo"))

    assert table.as_string() == "{}"

    with pytest.raises(NonExistentKey):
        table.remove(Key("foo"))


def test_inf_and_nan_are_supported(example):
    content = example("0.5.0")
    doc = parse(content)

    assert doc["sf1"] == float("inf")
    assert doc["sf2"] == float("inf")
    assert doc["sf3"] == float("-inf")

    assert math.isnan(doc["sf4"])
    assert math.isnan(doc["sf5"])
    assert math.isnan(doc["sf6"])


def test_hex_octal_and_bin_integers_are_supported(example):
    content = example("0.5.0")
    doc = parse(content)

    assert doc["hex1"] == 3735928559
    assert doc["hex2"] == 3735928559
    assert doc["hex3"] == 3735928559

    assert doc["oct1"] == 342391
    assert doc["oct2"] == 493

    assert doc["bin1"] == 214


def test_key_automatically_sets_proper_string_type_if_not_bare():
    key = Key("foo.bar")

    assert key.t == KeyType.Basic

    key = Key("")
    assert key.t == KeyType.Basic


def test_array_behaves_like_a_list():
    a = item([1, 2])

    assert a == [1, 2]
    assert a.as_string() == "[1, 2]"

    a += [3, 4]
    assert a == [1, 2, 3, 4]
    assert a.as_string() == "[1, 2, 3, 4]"

    del a[2]
    assert a == [1, 2, 4]
    assert a.as_string() == "[1, 2, 4]"

    assert a.pop() == 4
    assert a == [1, 2]
    assert a.as_string() == "[1, 2]"

    a[0] = 4
    assert a == [4, 2]
    a[-2] = 0
    assert a == [0, 2]

    del a[-2]
    assert a == [2]
    assert a.as_string() == "[2]"

    a.clear()
    assert a == []
    assert a.as_string() == "[]"

    content = """a = [1, 2,] # Comment
"""
    doc = parse(content)
    assert str(doc["a"]) == "[1, 2]"

    assert doc["a"] == [1, 2]
    doc["a"] += [3, 4]
    assert doc["a"] == [1, 2, 3, 4]
    assert (
        doc.as_string()
        == """a = [1, 2, 3, 4] # Comment
"""
    )


def test_array_multiline():
    t = item([1, 2, 3, 4, 5, 6, 7, 8])
    t.multiline(True)

    expected = """\
[
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
]"""

    assert expected == t.as_string()

    t = item([])
    t.multiline(True)

    assert t.as_string() == "[]"


def test_array_multiline_modify():
    doc = parse(
        """\
a = [
    "abc"
]"""
    )
    doc["a"].append("def")
    expected = """\
a = [
    "abc",
    "def"
]"""
    assert expected == doc.as_string()
    doc["a"].insert(1, "ghi")
    expected = """\
a = [
    "abc",
    "ghi",
    "def"
]"""
    assert expected == doc.as_string()


def test_append_to_empty_array():
    doc = parse("x = [ ]")
    doc["x"].append("a")
    assert doc.as_string() == 'x = ["a" ]'
    doc = parse("x = [\n]")
    doc["x"].append("a")
    assert doc.as_string() == 'x = [\n    "a"\n]'


def test_modify_array_with_comment():
    doc = parse("x = [ # comment\n]")
    doc["x"].append("a")
    assert doc.as_string() == 'x = [ # comment\n    "a"\n]'
    doc = parse(
        """\
x = [
    "a",
    # comment
    "b"
]"""
    )
    doc["x"].insert(1, "c")
    expected = """\
x = [
    "a",
    # comment
    "c",
    "b"
]"""
    assert doc.as_string() == expected
    doc = parse(
        """\
x = [
    1  # comment
]"""
    )
    doc["x"].append(2)
    assert (
        doc.as_string()
        == """\
x = [
    1,  # comment
    2
]"""
    )
    doc["x"].pop(0)
    assert doc.as_string() == "x = [\n    2\n]"


def test_append_to_multiline_array_with_comment():
    doc = parse(
        """\
x = [
    # Here is a comment
    1,
    2
]
"""
    )
    doc["x"].multiline(True).append(3)
    assert (
        doc.as_string()
        == """\
x = [
    # Here is a comment
    1,
    2,
    3,
]
"""
    )
    assert doc["x"].pop() == 3
    assert (
        doc.as_string()
        == """\
x = [
    # Here is a comment
    1,
    2,
]
"""
    )


def test_append_dict_to_array():
    doc = parse("x = []")
    doc["x"].append({"name": "John Doe", "email": "john@doe.com"})
    expected = 'x = [{name = "John Doe",email = "john@doe.com"}]'
    assert doc.as_string() == expected
    # Make sure the produced string is valid
    assert parse(doc.as_string()) == doc


def test_dicts_are_converted_to_tables():
    t = item({"foo": {"bar": "baz"}})

    assert (
        t.as_string()
        == """[foo]
bar = "baz"
"""
    )


def test_array_add_line():
    t = api.array()
    t.add_line(1, 2, 3, comment="Line 1")
    t.add_line(4, 5, 6, comment="Line 2")
    t.add_line(7, api.ws(","), api.ws(" "), 8, add_comma=False)
    t.add_line(comment="Line 4")
    t.add_line(indent="")
    assert len(t) == 8
    assert list(t) == [1, 2, 3, 4, 5, 6, 7, 8]
    assert (
        t.as_string()
        == """[
    1, 2, 3, # Line 1
    4, 5, 6, # Line 2
    7, 8,
    # Line 4
]"""
    )


def test_array_add_line_invalid_value():
    t = api.array()
    with pytest.raises(ValueError, match="is not allowed"):
        t.add_line(1, api.ws(" "))
    with pytest.raises(ValueError, match="is not allowed"):
        t.add_line(Comment(Trivia("  ", comment="test")))
    assert len(t) == 0


def test_dicts_are_converted_to_tables_and_keep_order():
    t = item(
        {
            "foo": {
                "bar": "baz",
                "abc": 123,
                "baz": [{"c": 3, "b": 2, "a": 1}],
            },
        }
    )

    assert (
        t.as_string()
        == """[foo]
bar = "baz"
abc = 123

[[foo.baz]]
c = 3
b = 2
a = 1
"""
    )


def test_dicts_are_converted_to_tables_and_are_sorted_if_requested():
    t = item(
        {
            "foo": {
                "bar": "baz",
                "abc": 123,
                "baz": [{"c": 3, "b": 2, "a": 1}],
            },
        },
        _sort_keys=True,
    )

    assert (
        t.as_string()
        == """[foo]
abc = 123
bar = "baz"

[[foo.baz]]
a = 1
b = 2
c = 3
"""
    )


def test_dicts_with_sub_dicts_are_properly_converted():
    t = item(
        {"foo": {"bar": {"string": "baz"}, "int": 34, "float": 3.14}}, _sort_keys=True
    )

    assert (
        t.as_string()
        == """[foo]
float = 3.14
int = 34

[foo.bar]
string = "baz"
"""
    )


def test_item_array_of_dicts_converted_to_aot():
    a = item({"foo": [{"bar": "baz"}]})

    assert (
        a.as_string()
        == """[[foo]]
bar = "baz"
"""
    )


def test_add_float_to_int():
    content = "[table]\nmy_int = 2043"
    doc = parse(content)
    doc["table"]["my_int"] += 5.0
    assert doc["table"]["my_int"] == 2048.0
    assert isinstance(doc["table"]["my_int"], float)


def test_sub_float_from_int():
    content = "[table]\nmy_int = 2048"
    doc = parse(content)
    doc["table"]["my_int"] -= 5.0
    assert doc["table"]["my_int"] == 2043.0
    assert isinstance(doc["table"]["my_int"], float)


def test_sub_int_from_float():
    content = "[table]\nmy_int = 2048.0"
    doc = parse(content)
    doc["table"]["my_int"] -= 5
    assert doc["table"]["my_int"] == 2043.0


def test_add_sum_int_with_float():
    content = "[table]\nmy_int = 2048.3"
    doc = parse(content)
    doc["table"]["my_int"] += 5
    assert doc["table"]["my_int"] == 2053.3


def test_integers_behave_like_ints():
    i = item(34)

    assert i == 34
    assert i.as_string() == "34"

    i += 1
    assert i == 35
    assert i.as_string() == "35"

    i -= 2
    assert i == 33
    assert i.as_string() == "33"

    i /= 2
    assert i == 16.5
    assert i.as_string() == "16.5"

    doc = parse("int = +34")
    doc["int"] += 1

    assert doc.as_string() == "int = +35"


def test_floats_behave_like_floats():
    i = item(34.12)

    assert i == 34.12
    assert i.as_string() == "34.12"

    i += 1
    assert i == 35.12
    assert i.as_string() == "35.12"

    i -= 2
    assert i == 33.12
    assert i.as_string() == "33.12"

    doc = parse("float = +34.12")
    doc["float"] += 1

    assert doc.as_string() == "float = +35.12"


def test_datetimes_behave_like_datetimes(tz_utc, tz_pst):
    i = item(datetime(2018, 7, 22, 12, 34, 56))

    assert i == datetime(2018, 7, 22, 12, 34, 56)
    assert i.as_string() == "2018-07-22T12:34:56"

    i += timedelta(days=1)
    assert i == datetime(2018, 7, 23, 12, 34, 56)
    assert i.as_string() == "2018-07-23T12:34:56"

    i -= timedelta(days=2)
    assert i == datetime(2018, 7, 21, 12, 34, 56)
    assert i.as_string() == "2018-07-21T12:34:56"

    i = i.replace(year=2019, tzinfo=tz_utc)
    assert i == datetime(2019, 7, 21, 12, 34, 56, tzinfo=tz_utc)
    assert i.as_string() == "2019-07-21T12:34:56+00:00"

    i = i.astimezone(tz_pst)
    assert i == datetime(2019, 7, 21, 4, 34, 56, tzinfo=tz_pst)
    assert i.as_string() == "2019-07-21T04:34:56-08:00"

    doc = parse("dt = 2018-07-22T12:34:56-05:00")
    doc["dt"] += timedelta(days=1)

    assert doc.as_string() == "dt = 2018-07-23T12:34:56-05:00"


def test_dates_behave_like_dates():
    i = item(date(2018, 7, 22))

    assert i == date(2018, 7, 22)
    assert i.as_string() == "2018-07-22"

    i += timedelta(days=1)
    assert i == datetime(2018, 7, 23)
    assert i.as_string() == "2018-07-23"

    i -= timedelta(days=2)
    assert i == date(2018, 7, 21)
    assert i.as_string() == "2018-07-21"

    i = i.replace(year=2019)
    assert i == datetime(2019, 7, 21)
    assert i.as_string() == "2019-07-21"

    doc = parse("dt = 2018-07-22 # Comment")
    doc["dt"] += timedelta(days=1)

    assert doc.as_string() == "dt = 2018-07-23 # Comment"


def test_parse_datetime_followed_by_space():
    # issue #260
    doc = parse("dt = 2018-07-22 ")
    assert doc["dt"] == date(2018, 7, 22)
    assert doc.as_string() == "dt = 2018-07-22 "

    doc = parse("dt = 2013-01-24 13:48:01.123456 ")
    assert doc["dt"] == datetime(2013, 1, 24, 13, 48, 1, 123456)
    assert doc.as_string() == "dt = 2013-01-24 13:48:01.123456 "


def test_times_behave_like_times():
    i = item(time(12, 34, 56))

    assert i == time(12, 34, 56)
    assert i.as_string() == "12:34:56"

    i = i.replace(hour=13)
    assert i == time(13, 34, 56)
    assert i.as_string() == "13:34:56"


def test_strings_behave_like_strs():
    i = item("foo")

    assert i == "foo"
    assert i.as_string() == '"foo"'

    i += " bar"
    assert i == "foo bar"
    assert i.as_string() == '"foo bar"'

    i += " é"
    assert i == "foo bar é"
    assert i.as_string() == '"foo bar é"'

    doc = parse('str = "foo" # Comment')
    doc["str"] += " bar"

    assert doc.as_string() == 'str = "foo bar" # Comment'


def test_string_add_preserve_escapes():
    i = api.value('"foo\\"bar"')
    i += " baz"

    assert i == 'foo"bar baz'
    assert i.as_string() == '"foo\\"bar baz"'


def test_tables_behave_like_dicts():
    t = item({"foo": "bar"})

    assert (
        t.as_string()
        == """foo = "bar"
"""
    )

    t.update({"bar": "baz"})

    assert (
        t.as_string()
        == """foo = "bar"
bar = "baz"
"""
    )

    t.update({"bar": "boom"})

    assert (
        t.as_string()
        == """foo = "bar"
bar = "boom"
"""
    )

    assert t.get("bar") == "boom"
    assert t.setdefault("foobar", "fuzz") == "fuzz"
    assert (
        t.as_string()
        == """foo = "bar"
bar = "boom"
foobar = "fuzz"
"""
    )


def test_items_are_pickable():
    n = item(12)

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "12"

    n = item(12.34)

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "12.34"

    n = item(True)

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "true"

    n = item(datetime(2018, 10, 11, 12, 34, 56, 123456))

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "2018-10-11T12:34:56.123456"

    n = item(date(2018, 10, 11))

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "2018-10-11"

    n = item(time(12, 34, 56, 123456))

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "12:34:56.123456"

    n = item([1, 2, 3])

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == "[1, 2, 3]"

    n = item({"foo": "bar"})

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == 'foo = "bar"\n'

    n = api.inline_table()
    n["foo"] = "bar"

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == '{foo = "bar"}'

    n = item("foo")

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == '"foo"'

    n = item([{"foo": "bar"}])

    s = pickle.dumps(n)
    assert pickle.loads(s).as_string() == 'foo = "bar"\n'


def test_trim_comments_when_building_inline_table():
    table = api.inline_table()
    row = parse('foo = "bar"  # Comment')
    table.update(row)
    assert table.as_string() == '{foo = "bar"}'
    value = item("foobaz")
    value.comment("Another comment")
    table.append("baz", value)
    assert "# Another comment" not in table.as_string()
    assert table.as_string() == '{foo = "bar", baz = "foobaz"}'


def test_deleting_inline_table_element_does_not_leave_trailing_separator():
    table = api.inline_table()
    table["foo"] = "bar"
    table["baz"] = "boom"

    assert table.as_string() == '{foo = "bar", baz = "boom"}'

    del table["baz"]

    assert table.as_string() == '{foo = "bar"}'

    table = api.inline_table()
    table["foo"] = "bar"

    del table["foo"]

    table["baz"] = "boom"

    assert table.as_string() == '{baz = "boom"}'


def test_deleting_inline_table_element_does_not_leave_trailing_separator2():
    doc = parse('a = {foo = "bar", baz = "boom"}')
    table = doc["a"]
    assert table.as_string() == '{foo = "bar", baz = "boom"}'

    del table["baz"]
    assert table.as_string() == '{foo = "bar" }'

    del table["foo"]
    assert table.as_string() == "{ }"

    table["baz"] = "boom"

    assert table.as_string() == '{ baz = "boom"}'


def test_booleans_comparison():
    boolean = Bool(True, Trivia())

    assert boolean

    boolean = Bool(False, Trivia())

    assert not boolean

    s = """[foo]
value = false
"""

    content = parse(s)

    assert {"foo": {"value": False}} == content
    assert {"value": False} == content["foo"]


def test_table_copy():
    table = item({"foo": "bar"})
    table_copy = table.copy()
    assert isinstance(table_copy, Table)
    table["foo"] = "baz"
    assert table_copy["foo"] == "bar"
    assert table_copy.as_string() == 'foo = "bar"\n'


def test_copy_copy():
    result = parse(
        """
    [tool.poetry]
    classifiers = [
    # comment
        "a",
        "b",
    ]
    """
    )
    classifiers = result["tool"]["poetry"]["classifiers"]
    new = copy.copy(classifiers)
    assert new == classifiers


@pytest.mark.parametrize(
    "key_str,escaped",
    [("\\", '"\\\\"'), ('"', '"\\""'), ("\t", '"\\t"'), ("\x10", '"\\u0010"')],
)
def test_escape_key(key_str, escaped):
    assert api.key(key_str).as_string() == escaped


def test_custom_encoders():
    import decimal

    @api.register_encoder
    def encode_decimal(obj):
        if isinstance(obj, decimal.Decimal):
            return api.float_(str(obj))
        raise TypeError

    assert api.item(decimal.Decimal("1.23")).as_string() == "1.23"

    with pytest.raises(TypeError):
        api.item(object())

    assert api.dumps({"foo": decimal.Decimal("1.23")}) == "foo = 1.23\n"
    api.unregister_encoder(encode_decimal)
