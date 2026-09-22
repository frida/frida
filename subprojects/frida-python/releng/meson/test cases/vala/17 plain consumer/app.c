#include "badger.h"

int main(int argc, char *argv[]) {
    Badger *badger;

    badger = g_object_new(TYPE_BADGER, NULL);
    g_print("Badger whose name is '%s'\n", badger_get_name(badger));
    g_object_unref(badger);

    return 0;
}
