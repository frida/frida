import datetime

from tomlkit import aot
from tomlkit import array
from tomlkit import comment
from tomlkit import document
from tomlkit import item
from tomlkit import nl
from tomlkit import parse
from tomlkit import table
from tomlkit._utils import _utc


def test_build_example(example):
    content = example("example")

    doc = document()
    doc.add(comment("This is a TOML document. Boom."))
    doc.add(nl())
    doc.add("title", "TOML Example")

    owner = table()
    owner.add("name", "Tom Preston-Werner")
    owner.add("organization", "GitHub")
    owner.add("bio", "GitHub Cofounder & CEO\nLikes tater tots and beer.")
    owner.add("dob", datetime.datetime(1979, 5, 27, 7, 32, tzinfo=_utc))
    owner["dob"].comment("First class dates? Why not?")

    doc.add("owner", owner)

    database = table()
    database["server"] = "192.168.1.1"
    database["ports"] = [8001, 8001, 8002]
    database["connection_max"] = 5000
    database["enabled"] = True

    doc["database"] = database

    servers = table()
    servers.add(nl())
    c = comment(
        "You can indent as you please. Tabs or spaces. TOML don't care."
    ).indent(2)
    c.trivia.trail = ""
    servers.add(c)
    alpha = table()
    servers.append("alpha", alpha)
    alpha.indent(2)
    alpha.add("ip", "10.0.0.1")
    alpha.add("dc", "eqdc10")

    beta = table()
    servers.append("beta", beta)
    beta.add("ip", "10.0.0.2")
    beta.add("dc", "eqdc10")
    beta.add("country", "中国")
    beta["country"].comment("This should be parsed as UTF-8")
    beta.indent(2)

    doc["servers"] = servers

    clients = table()
    doc.add("clients", clients)
    clients["data"] = item([["gamma", "delta"], [1, 2]]).comment(
        "just an update to make sure parsers support it"
    )

    clients.add(nl())
    clients.add(comment("Line breaks are OK when inside arrays"))
    clients["hosts"] = array(
        """[
  "alpha",
  "omega"
]"""
    )

    doc.add(nl())
    doc.add(comment("Products"))

    products = aot()
    doc["products"] = products

    hammer = table().indent(2)
    hammer["name"] = "Hammer"
    hammer["sku"] = 738594937

    nail = table().indent(2)
    nail["name"] = "Nail"
    nail["sku"] = 284758393
    nail["color"] = "gray"

    products.append(hammer)
    products.append(nail)

    assert content == doc.as_string()


def test_add_remove():
    content = ""

    doc = parse(content)
    doc.append("foo", "bar")

    assert (
        doc.as_string()
        == """foo = "bar"
"""
    )

    doc.remove("foo")

    assert doc.as_string() == ""


def test_append_table_after_multiple_indices():
    content = """
    [packages]
    foo = "*"

    [settings]
    enable = false

    [packages.bar]
    version = "*"
    """
    doc = parse(content)
    doc.append("foobar", {"name": "John"})


def test_top_level_keys_are_put_at_the_root_of_the_document():
    doc = document()
    doc.add(comment("Comment"))
    doc["foo"] = {"name": "test"}
    doc["bar"] = 1

    expected = """\
# Comment
bar = 1

[foo]
name = "test"
"""

    assert doc.as_string() == expected
