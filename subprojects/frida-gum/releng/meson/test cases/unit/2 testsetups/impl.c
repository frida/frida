/* Write past the end. */

void do_nasty(char *ptr) {
    ptr[10] = 'n';
}
