import os

from tomlkit.toml_document import TOMLDocument
from tomlkit.toml_file import TOMLFile


def test_toml_file(example):
    original_content = example("example")

    toml_file = os.path.join(os.path.dirname(__file__), "examples", "example.toml")
    toml = TOMLFile(toml_file)

    content = toml.read()
    assert isinstance(content, TOMLDocument)
    assert content["owner"]["organization"] == "GitHub"

    toml.write(content)

    try:
        with open(toml_file, encoding="utf-8") as f:
            assert original_content == f.read()
    finally:
        with open(toml_file, "w", encoding="utf-8", newline="") as f:
            assert f.write(original_content)


def test_keep_old_eol(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    with open(toml_path, "wb+") as f:
        f.write(b"a = 1\r\nb = 2\r\n")

    f = TOMLFile(toml_path)
    content = f.read()
    content["b"] = 3
    f.write(content)

    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1\r\nb = 3\r\n"


def test_keep_old_eol_2(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    with open(toml_path, "wb+") as f:
        f.write(b"a = 1\nb = 2\n")

    f = TOMLFile(toml_path)
    content = f.read()
    content["b"] = 3
    f.write(content)

    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1\nb = 3\n"


def test_mixed_eol(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    with open(toml_path, "wb+") as f:
        f.write(b"a = 1\r\nrb = 2\n")

    f = TOMLFile(toml_path)
    f.write(f.read())

    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1\r\nrb = 2\n"


def test_consistent_eol(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    with open(toml_path, "wb+") as f:
        f.write(b"a = 1\r\nb = 2\r\n")

    f = TOMLFile(toml_path)
    content = f.read()
    content["c"] = 3
    f.write(content)

    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1\r\nb = 2\r\nc = 3\r\n"


def test_consistent_eol_2(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    with open(toml_path, "wb+") as f:
        f.write(b"a = 1\nb = 2\n")

    f = TOMLFile(toml_path)
    content = f.read()
    content["c"] = 3
    content["c"].trivia.trail = "\r\n"
    f.write(content)

    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1\nb = 2\nc = 3\n"


def test_default_eol_is_os_linesep(tmpdir):
    toml_path = str(tmpdir / "pyproject.toml")
    f = TOMLFile(toml_path)
    content = TOMLDocument()
    content.append("a", 1)
    content["a"].trivia.trail = "\n"
    content.append("b", 2)
    content["b"].trivia.trail = "\r\n"
    f.write(content)
    linesep = os.linesep.encode()
    with open(toml_path, "rb") as f:
        assert f.read() == b"a = 1" + linesep + b"b = 2" + linesep
