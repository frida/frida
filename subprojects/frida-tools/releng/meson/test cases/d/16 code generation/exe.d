module exe;

import generated;
import std.stdio;

int main()
{
    return generatedString() == "Some text to be returned by generated code" ? 0 : 1;
}
