
extern(C) int printLibraryString(const char *str);

void main ()
{
    immutable ret = printLibraryString ("C foo");
    assert (ret == 3);
}
