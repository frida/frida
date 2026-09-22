module test;


int main()
{
    version(TestVersion)
    {
        enum testPhrase = import("test.txt");
        return testPhrase == "TEST PHRASE" ? 0 : 1;
    }
    else
    {
        return 1;
    }
}
