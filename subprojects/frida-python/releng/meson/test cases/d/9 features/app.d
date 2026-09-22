
import std.stdio;
import std.array : split;
import std.string : strip;

import extra;

auto getMenu ()
{
    auto foods = import ("food.txt").strip.split ("\n");
    return foods;
}

auto getPeople ()
{
    return import ("people.txt").strip.split ("\n");
}

void main (string[] args)
{
    import std.array : join;
    import core.stdc.stdlib : exit;

    immutable request = args[1];
    if (request == "menu") {
        version (No_Menu) {
        } else {
            writeln ("On the menu: ", getMenu.join (", "));
            exit (0);
        }
    }

    version (With_People) {
        if (request == "people") {
            writeln ("People: ", getPeople.join (", "));

            // only exit successfully if the second module also had its module version set.
            // this checks for issue https://github.com/mesonbuild/meson/issues/3337
            if (secondModulePeopleVersionSet ())
                exit (0);
            exit (1);
        }
    }

    version (With_VersionInteger)
        version(3) exit(0);

    version (With_Debug)
        debug exit(0);

    version (With_DebugInteger)
        debug(3) exit(0);

    version (With_DebugIdentifier)
        debug(DebugIdentifier) exit(0);

    version (With_DebugAll) {
        int dbg = 0;
        debug dbg++;
        debug(2) dbg++;
        debug(3) dbg++;
        debug(4) dbg++;
        debug(DebugIdentifier) dbg++;

        if (dbg == 5)
            exit(0);
    }

    // we fail here
    exit (1);
}

unittest
{
    writeln ("TEST");
    import core.stdc.stdlib : exit;

    writeln(getMenu);
    assert (getMenu () == ["Spam", "Eggs", "Spam", "Baked Beans", "Spam", "Spam"]);

    exit (0);
}
