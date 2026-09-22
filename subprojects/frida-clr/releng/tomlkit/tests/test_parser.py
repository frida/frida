import pytest

from tomlkit.exceptions import EmptyTableNameError
from tomlkit.exceptions import InternalParserError
from tomlkit.exceptions import UnexpectedCharError
from tomlkit.items import StringType
from tomlkit.parser import Parser


def test_parser_should_raise_an_internal_error_if_parsing_wrong_type_of_string():
    parser = Parser('"foo"')

    with pytest.raises(InternalParserError) as e:
        parser._parse_string(StringType.SLL)

    assert e.value.line == 1
    assert e.value.col == 0


def test_parser_should_raise_an_error_for_empty_tables():
    content = """
[one]
[]
"""

    parser = Parser(content)

    with pytest.raises(EmptyTableNameError) as e:
        parser.parse()

    assert e.value.line == 3
    assert e.value.col == 1


def test_parser_should_raise_an_error_if_equal_not_found():
    content = """[foo]
a {c = 1, d = 2}
"""
    parser = Parser(content)
    with pytest.raises(UnexpectedCharError):
        parser.parse()


def test_parse_multiline_string_ignore_the_first_newline():
    content = 'a = """\nfoo\n"""'
    parser = Parser(content)
    assert parser.parse() == {"a": "foo\n"}

    content = 'a = """\r\nfoo\n"""'
    parser = Parser(content)
    assert parser.parse() == {"a": "foo\n"}
