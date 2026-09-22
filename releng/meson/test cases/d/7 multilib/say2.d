
import std.stdio;
import std.string : format;

export int sayHello2 (string str)
{
    writeln ("Hello %s from library 2.".format (str));
    return 8;
}

version (Windows)
{
    import core.sys.windows.dll;
    mixin SimpleDllMain;
}
