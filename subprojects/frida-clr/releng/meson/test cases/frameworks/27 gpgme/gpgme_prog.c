#include <gpgme.h>

int
main()
{
    printf("gpgme-v%s", gpgme_check_version(NULL));
    return 0;
}
