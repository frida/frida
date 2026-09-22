Quickstart
==========

Parsing
-------

TOML Kit comes with a fast and style-preserving parser to help you access
the content of TOML files and strings::


    >>> from tomlkit import dumps
    >>> from tomlkit import parse  # you can also use loads

    >>> content = """[table]
    ... foo = "bar"  # String
    ... """
    >>> doc = parse(content)

    # doc is a TOMLDocument instance that holds all the information
    # about the TOML string.
    # It behaves like a standard dictionary.

    >>> assert doc["table"]["foo"] == "bar"

    # The string generated from the document is exactly the same
    # as the original string
    >>> assert dumps(doc) == content


Modifying
---------

TOML Kit provides an intuitive API to modify TOML documents::

    >>> from tomlkit import dumps
    >>> from tomlkit import parse
    >>> from tomlkit import table

    >>> doc = parse("""[table]
    ... foo = "bar"  # String
    ... """)

    >>> doc["table"]["baz"] = 13

    >>> dumps(doc)
    """[table]
    foo = "bar"  # String
    baz = 13
    """

    # Add a new table
    >>> tab = table()
    >>> tab.add("array", [1, 2, 3])

    >>> doc["table2"] = tab

    >>> dumps(doc)
    """[table]
    foo = "bar"  # String
    baz = 13

    [table2]
    array = [1, 2, 3]
    """

    # Remove the newly added table
    >>> doc.pop("table2")
    # del doc["table2] is also possible

Writing
-------

You can also write a new TOML document from scratch.

Let's say we want to create this following document

.. code-block:: toml

    # This is a TOML document.

    title = "TOML Example"

    [owner]
    name = "Tom Preston-Werner"
    organization = "GitHub"
    bio = "GitHub Cofounder & CEO\nLikes tater tots and beer."
    dob = 1979-05-27T07:32:00Z # First class dates? Why not?

    [database]
    server = "192.168.1.1"
    ports = [ 8001, 8001, 8002 ]
    connection_max = 5000
    enabled = true

It can be created with the following code::

    >>> from tomlkit import comment
    >>> from tomlkit import document
    >>> from tomlkit import nl
    >>> from tomlkit import table

    >>> doc = document()
    >>> doc.add(comment("This is a TOML document."))
    >>> doc.add(nl())
    >>> doc.add("title", "TOML Example")
    # Using doc["title"] = "TOML Example" is also possible

    >>> owner = table()
    >>> owner.add("name", "Tom Preston-Werner")
    >>> owner.add("organization", "GitHub")
    >>> owner.add("bio", "GitHub Cofounder & CEO\nLikes tater tots and beer.")
    >>> owner.add("dob", datetime(1979, 5, 27, 7, 32, tzinfo=utc))
    >>> owner["dob"].comment("First class dates? Why not?")

    # Adding the table to the document
    >>> doc.add("owner", owner)

    >>> database = table()
    >>> database["server"] = "192.168.1.1"
    >>> database["ports"] = [8001, 8001, 8002]
    >>> database["connection_max"] = 5000
    >>> database["enabled"] = True

    >>> doc["database"] = database
