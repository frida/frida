
void secondModuleTestFunc ()
{
    import std.stdio : writeln;

    version (unittest)
        writeln ("Hello!");
    else
        assert (0);
}
