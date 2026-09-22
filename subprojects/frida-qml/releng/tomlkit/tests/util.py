from tomlkit.items import AoT
from tomlkit.items import Array
from tomlkit.items import Bool
from tomlkit.items import Comment
from tomlkit.items import Date
from tomlkit.items import DateTime
from tomlkit.items import Float
from tomlkit.items import InlineTable
from tomlkit.items import Integer
from tomlkit.items import Item
from tomlkit.items import KeyType
from tomlkit.items import Null
from tomlkit.items import SingleKey as Key
from tomlkit.items import String
from tomlkit.items import StringType
from tomlkit.items import Table
from tomlkit.items import Time
from tomlkit.items import Trivia
from tomlkit.toml_document import TOMLDocument


TOMLKIT_TYPES = [
    Bool,
    Comment,
    InlineTable,
    Integer,
    Float,
    DateTime,
    Date,
    Time,
    Array,
    KeyType,
    Key,
    String,
    StringType,
    Table,
    Trivia,
    Item,
    AoT,
    Null,
    TOMLDocument,
]


def assert_not_tomlkit_type(v):
    for _, tomlkit_type in enumerate(TOMLKIT_TYPES):
        assert not isinstance(v, tomlkit_type)


def assert_is_ppo(v_unwrapped, unwrapped_type):
    assert_not_tomlkit_type(v_unwrapped)
    assert isinstance(v_unwrapped, unwrapped_type)


def elementary_test(v, unwrapped_type):
    v_unwrapped = v.unwrap()
    assert_is_ppo(v_unwrapped, unwrapped_type)
