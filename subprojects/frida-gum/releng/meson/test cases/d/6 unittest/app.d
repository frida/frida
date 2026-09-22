
import std.stdio;

uint getFour ()
{
    auto getTwo ()
    {
        return 1 + 1;
    }

    return getTwo () + getTwo ();
}

void main ()
{
    import core.stdc.stdlib : exit;

    writeln ("Four: ", getFour ());
    exit (4);
}

unittest
{
    writeln ("TEST");
    import core.stdc.stdlib : exit;
    import second_unit;

    assert (getFour () > 2);
    assert (getFour () == 4);

    // this is a regression test for https://github.com/mesonbuild/meson/issues/3337
    secondModuleTestFunc ();

    // we explicitly terminate here to give the unittest program a different exit
    // code than the main application has.
    // (this prevents the regular main() from being executed)
    exit (0);
}
