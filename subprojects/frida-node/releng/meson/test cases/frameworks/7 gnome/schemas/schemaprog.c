#include<gio/gio.h>
#include<stdio.h>
#include<string.h>

int main(int argc, char **argv) {
    GSettingsSchemaSource *src;
    GSettingsSchema *schema;
    GSettings *settings;
    GVariant *value;

    GError *error = NULL;
    src = g_settings_schema_source_new_from_directory("schemas",
            g_settings_schema_source_get_default(), TRUE, &error);
    if(error) {
        fprintf(stderr, "Fail: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    schema = g_settings_schema_source_lookup(src, "com.github.meson", FALSE);
    if(!schema) {
        fprintf(stderr, "Could not get schema from source.\n");
        return 2;
    }

    settings = g_settings_new_full(schema, NULL, NULL);
    if(!settings) {
        fprintf(stderr, "Could not get settings object.\n");
        return 3;
    }

    value = g_settings_get_value(settings, "greeting");
    if(!value) {
        fprintf(stderr, "Could not get value from settings.\n");
        return 4;
    }

    if(strcmp("Hello", g_variant_get_string(value, NULL)) != 0) {
        fprintf(stderr, "Value of setting is incorrect.\n");
        return 5;
    }
    g_variant_unref(value);
    g_object_unref(settings);
    g_settings_schema_unref(schema);
    g_settings_schema_source_unref(src);
    return 0;
}
