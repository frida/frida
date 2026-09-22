/* vim: set sts=4 sw=4 et : */

#include <stdio.h>
#include <SDL_version.h>

int main(int argc, char *argv[]) {
    SDL_version compiled;
    SDL_version linked;

    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);

    if (compiled.major != linked.major) {
        fprintf(stderr, "Compiled major '%u' != linked major '%u'",
                compiled.major, linked.major);
        return -1;
    }

    if (compiled.minor != linked.minor) {
        fprintf(stderr, "Compiled minor '%u' != linked minor '%u'",
                compiled.minor, linked.minor);
        return -2;
    }
#if 0
    /* Disabled because sometimes this is 'micro' and sometimes 'patch' */
    if (compiled.micro != linked.micro) {
        fprintf(stderr, "Compiled micro '%u' != linked micro '%u'",
                compiled.micro, linked.micro);
        return -3;
    }
#endif
    return 0;
}
