int bar_built_value (int in);

int main (int argc, char *argv[])
{
    // this will evaluate to 0
    return bar_built_value(10) - (42 + 1969 + 10);
}
