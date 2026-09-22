
import std.stdio;
import std.string : format;

@safe
export int printLibraryString (string str)
{
    writeln ("Library says: %s".format (str));
    return 4;
}

version (Windows)
{
    import core.sys.windows.dll;
    mixin SimpleDllMain;
}
