#include <foo.h>
#include <stdio.h>

int
main (int argc, char * argv[])
{
    int value = power_level ();
    if (value < 9000) {
        printf ("Power level is %i\n", value);
        return 1;
    }
    printf ("IT'S OVER 9000!!!\n");
    return 0;
}
