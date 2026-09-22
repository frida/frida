module test_sizeof;

alias voidp = void*;

int main()
{
    version(Is64bits) {
        enum expectedSz = 8;
    }
    else version(Is32bits) {
        enum expectedSz = 4;
    }
    else {
        assert(false, "No version set!");
    }
    return expectedSz == voidp.sizeof ? 0 : 1;
}
