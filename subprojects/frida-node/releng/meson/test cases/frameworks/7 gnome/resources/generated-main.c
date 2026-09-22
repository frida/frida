#include<stdio.h>
#include<string.h>
#include<gio/gio.h>
#include"generated-resources.h"

#define EXPECTED "This is a generated resource.\n"

int main(int argc, char **argv) {
    generated_resources_get_resource();
    GError *err = NULL;
    GBytes *data = g_resources_lookup_data("/com/example/myprog/res3.txt",
            G_RESOURCE_LOOKUP_FLAGS_NONE, &err);

    if(data == NULL) {
        fprintf(stderr, "Data lookup failed: %s\n", err->message);
        return 1;
    }
    if(strcmp(g_bytes_get_data(data, NULL), EXPECTED) != 0) {
        fprintf(stderr, "Resource contents are wrong:\n %s\n",
                (const char*)g_bytes_get_data(data, NULL));
        return 1;
    }
    fprintf(stdout, "All ok.\n");
    g_bytes_unref(data);
    return 0;
}
