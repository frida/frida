module generator;

import std.file;
import std.stdio;
import std.string;

void main(string[] args)
{
    const text = cast(string)read(args[1]);

    writeln("module generated;");
    writefln!`string generatedString() { return "%s"; }`(text.strip());
}
