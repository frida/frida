int main(int argc, char **argv)
{
    int (^callback)(void) = ^ int (void) { return 0; };

    return callback();
}
