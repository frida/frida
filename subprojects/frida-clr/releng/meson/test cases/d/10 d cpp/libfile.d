import std.stdio;

extern (C++) void print_hello(int i) {
    writefln("Hello. Here is a number printed with D: %d", i);
}
