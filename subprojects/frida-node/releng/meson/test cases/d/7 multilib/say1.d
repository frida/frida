
import std.stdio;
import std.string : format;

export int sayHello1 (string str)
{
    writeln ("Hello %s from library 1.".format (str));
    return 4;
}

version (Windows)
{
    import core.sys.windows.dll;
    mixin SimpleDllMain;
}
