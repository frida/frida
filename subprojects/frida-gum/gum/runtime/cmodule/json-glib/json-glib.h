#ifndef __JSON_GLIB_H__
#define __JSON_GLIB_H__

#include <glib.h>

typedef struct _JsonBuilder JsonBuilder;
typedef struct _JsonNode JsonNode;

JsonBuilder * json_builder_new_immutable (void);

JsonNode * json_builder_get_root (JsonBuilder * builder);
void json_builder_reset (JsonBuilder * builder);

JsonBuilder * json_builder_begin_array (JsonBuilder * builder);
JsonBuilder * json_builder_end_array (JsonBuilder * builder);
JsonBuilder * json_builder_begin_object (JsonBuilder * builder);
JsonBuilder * json_builder_end_object (JsonBuilder * builder);

JsonBuilder * json_builder_set_member_name (JsonBuilder * builder,
    const gchar * member_name);
JsonBuilder * json_builder_add_int_value (JsonBuilder * builder, gint64 value);
JsonBuilder * json_builder_add_double_value (JsonBuilder * builder,
    gdouble value);
JsonBuilder * json_builder_add_boolean_value (JsonBuilder * builder,
    gboolean value);
JsonBuilder * json_builder_add_string_value (JsonBuilder * builder,
    const gchar * value);
JsonBuilder * json_builder_add_null_value (JsonBuilder * builder);

JsonNode * json_node_ref (JsonNode * node);
void json_node_unref (JsonNode * node);

char * json_to_string (JsonNode * node, gboolean pretty);

#endif
