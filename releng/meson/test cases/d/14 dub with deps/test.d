module test;

// testing import dirs
import xlsx;

// dependency of xlsx
import dxml.dom;

const xml = "<!-- comment -->\n" ~
    "<root>\n" ~
    "    <foo>some text<whatever/></foo>\n" ~
    "    <bar/>\n" ~
    "    <baz></baz>\n" ~
    "</root>";

int main()
{
    // testing versions
    version (Have_dxml)
    {
        import std.range.primitives : empty;

        auto dom = parseDOM(xml);
        assert(dom.type == EntityType.elementStart);
        assert(dom.name.empty);
        assert(dom.children.length == 2);

        assert(dom.children[0].type == EntityType.comment);
        assert(dom.children[0].text == " comment ");

        auto root = dom.children[1];
        assert(root.type == EntityType.elementStart);
        assert(root.name == "root");
        assert(root.children.length == 3);

        auto foo = root.children[0];
        assert(foo.type == EntityType.elementStart);
        assert(foo.name == "foo");
        assert(foo.children.length == 2);

        assert(foo.children[0].type == EntityType.text);
        assert(foo.children[0].text == "some text");

        assert(foo.children[1].type == EntityType.elementEmpty);
        assert(foo.children[1].name == "whatever");

        assert(root.children[1].type == EntityType.elementEmpty);
        assert(root.children[1].name == "bar");

        assert(root.children[2].type == EntityType.elementStart);
        assert(root.children[2].name == "baz");
        assert(root.children[2].children.length == 0);
        return 0;
    }
    else
    {
        return 1;
    }
}
