/*
 * Copyright (C) 2016-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsourcemap.h"

#include <json-glib/json-glib.h>

typedef struct _GumSourceMapping GumSourceMapping;

struct _GumSourceMap
{
  GObject parent;

  GPtrArray * sources;
  GPtrArray * names;
  GArray * mappings;
};

struct _GumSourceMapping
{
  guint generated_line;
  guint generated_column;

  const gchar * source;
  guint line;
  guint column;
  const gchar * name;
};

static void gum_source_map_finalize (GObject * object);
static gboolean gum_source_map_load (GumSourceMap * self, const gchar * json);
static gboolean gum_source_map_load_mappings (GumSourceMap * self,
    const gchar * encoded_mappings);

static gconstpointer gum_bsearch_find_closest (gconstpointer needle,
    GArray * haystack, GCompareFunc compare);
static gint gum_find_closest_in_range (gint low, gint high,
    gconstpointer needle, GArray * haystack, GCompareFunc compare);
static gint gum_source_mapping_compare (const GumSourceMapping * a,
    const GumSourceMapping * b);

static gboolean gum_read_string_array (JsonReader * reader,
    const gchar * member_name, GPtrArray * array);

static gboolean gum_parse_segment (const gchar ** cursor, gint * segment,
    guint * segment_length);
static gboolean gum_parse_vlq_value (const gchar ** cursor, gint * value);

static gboolean gum_line_starts_with_source_map_comment (const gchar * line,
    gsize len);

G_DEFINE_TYPE (GumSourceMap, gum_source_map, G_TYPE_OBJECT)

static void
gum_source_map_class_init (GumSourceMapClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_source_map_finalize;
}

static void
gum_source_map_init (GumSourceMap * self)
{
  self->sources = g_ptr_array_new_with_free_func (g_free);
  self->names = g_ptr_array_new_with_free_func (g_free);
  self->mappings = g_array_new (FALSE, FALSE, sizeof (GumSourceMapping));
}

static void
gum_source_map_finalize (GObject * object)
{
  GumSourceMap * self = GUM_SOURCE_MAP (object);

  g_array_unref (self->mappings);
  g_ptr_array_unref (self->names);
  g_ptr_array_unref (self->sources);

  G_OBJECT_CLASS (gum_source_map_parent_class)->finalize (object);
}

GumSourceMap *
gum_source_map_new (const gchar * json)
{
  GumSourceMap * map;

  map = g_object_new (GUM_TYPE_SOURCE_MAP, NULL);

  if (!gum_source_map_load (map, json))
  {
    g_object_unref (map);
    return NULL;
  }

  return map;
}

static gboolean
gum_source_map_load (GumSourceMap * self,
                     const gchar * json)
{
  JsonNode * root;
  JsonReader * reader;
  const gchar * mappings;

  root = json_from_string (json, NULL);
  if (root == NULL)
    return FALSE;
  reader = json_reader_new (root);
  json_node_unref (root);

  if (!gum_read_string_array (reader, "sources", self->sources))
    goto error;

  gum_read_string_array (reader, "names", self->names);

  json_reader_read_member (reader, "mappings");
  mappings = json_reader_get_string_value (reader);
  if (mappings == NULL)
    goto error;
  if (!gum_source_map_load_mappings (self, mappings))
    goto error;
  json_reader_end_member (reader);

  g_object_unref (reader);
  return TRUE;

error:
  {
    g_object_unref (reader);
    return FALSE;
  }
}

static gboolean
gum_source_map_load_mappings (GumSourceMap * self,
                              const gchar * encoded_mappings)
{
  GPtrArray * sources = self->sources;
  GPtrArray * names = self->names;
  GArray * mappings = self->mappings;
  const gchar * cursor = encoded_mappings;
  guint generated_line = 1;
  gint prev_generated_column = 0;
  gint prev_source = 0;
  gint prev_line = 0;
  gint prev_column = 0;
  gint prev_name = 0;

  while (*cursor != '\0')
  {
    GumSourceMapping * mapping;
    guint mapping_index;
    gint segment[5];
    guint segment_length;

    if (*cursor == ';')
    {
      generated_line++;
      prev_generated_column = 0;
      cursor++;
      continue;
    }
    else if (*cursor == ',')
    {
      cursor++;
      continue;
    }

    mapping_index = mappings->len;
    g_array_set_size (mappings, mapping_index + 1);
    mapping = &g_array_index (mappings, GumSourceMapping, mapping_index);

    mapping->generated_line = generated_line;

    if (!gum_parse_segment (&cursor, segment, &segment_length))
      return FALSE;

    mapping->generated_column = prev_generated_column + segment[0];
    prev_generated_column = mapping->generated_column;

    if (segment_length > 1)
    {
      gint source_index;

      source_index = prev_source + segment[1];
      if (source_index < 0 || source_index >= (gint) sources->len)
        return FALSE;
      mapping->source = g_ptr_array_index (sources, source_index);
      prev_source = source_index;

      mapping->line = prev_line + segment[2];
      prev_line = mapping->line;
      mapping->line++;

      mapping->column = prev_column + segment[3];
      prev_column = mapping->column;

      if (segment_length > 4)
      {
        gint name_index;

        name_index = prev_name + segment[4];
        if (name_index < 0 || name_index >= (gint) names->len)
          return FALSE;
        mapping->name = g_ptr_array_index (names, name_index);
        prev_name = name_index;
      }
      else
      {
        mapping->name = NULL;
      }
    }
    else
    {
      mapping->source = NULL;
      mapping->line = 0;
      mapping->column = 0;
      mapping->name = NULL;
    }
  }

  g_array_sort (mappings, (GCompareFunc) gum_source_mapping_compare);

  return TRUE;
}

gboolean
gum_source_map_resolve (GumSourceMap * self,
                        guint * line,
                        guint * column,
                        const gchar ** source,
                        const gchar ** name)
{
  GumSourceMapping needle;
  const GumSourceMapping * mapping;

  needle.generated_line = *line;
  needle.generated_column = *column;

  mapping = gum_bsearch_find_closest (&needle, self->mappings,
      (GCompareFunc) gum_source_mapping_compare);
  if (mapping == NULL || mapping->generated_line != needle.generated_line)
    return FALSE;

  *line = mapping->line;
  *column = mapping->column;
  *source = mapping->source;
  *name = mapping->name;

  return TRUE;
}

static gconstpointer
gum_bsearch_find_closest (gconstpointer needle,
                          GArray * haystack,
                          GCompareFunc compare)
{
  gint index;
  guint element_size;

  if (haystack->len == 0)
    return NULL;

  index =
      gum_find_closest_in_range (-1, haystack->len, needle, haystack, compare);
  if (index < 0)
    return NULL;

  element_size = g_array_get_element_size (haystack);

  while (index - 1 >= 0)
  {
    if (compare (haystack->data + (index * element_size),
        haystack->data + ((index - 1) * element_size)) != 0)
      break;
    index--;
  }

  return haystack->data + (index * element_size);
}

static gint
gum_find_closest_in_range (gint low,
                           gint high,
                           gconstpointer needle,
                           GArray * haystack,
                           GCompareFunc compare)
{
  gint mid, comparison;

  mid = ((high - low) / 2) + low;

  comparison = compare (needle,
      haystack->data + (mid * g_array_get_element_size (haystack)));
  if (comparison == 0)
  {
    return mid;
  }
  else if (comparison > 0)
  {
    if (high - mid > 1)
      return gum_find_closest_in_range (mid, high, needle, haystack, compare);
    else
      return mid;
  }
  else
  {
    if (mid - low > 1)
      return gum_find_closest_in_range (low, mid, needle, haystack, compare);
    else
      return low < 0 ? -1 : low;
  }
}

static gint
gum_source_mapping_compare (const GumSourceMapping * a,
                            const GumSourceMapping * b)
{
  gint result;

  result = a->generated_line - b->generated_line;
  if (result != 0)
    return result;

  if (a->generated_column == G_MAXUINT || b->generated_column == G_MAXUINT)
    return 0;

  result = a->generated_column - b->generated_column;

  return result;
}

static gboolean
gum_read_string_array (JsonReader * reader,
                       const gchar * member_name,
                       GPtrArray * array)
{
  gint num_elements, element_index;

  if (!json_reader_read_member (reader, member_name))
    goto member_error;

  num_elements = json_reader_count_elements (reader);
  if (num_elements == -1)
    goto member_error;

  g_ptr_array_set_size (array, num_elements);

  for (element_index = 0; element_index != num_elements; element_index++)
  {
    const gchar * element;

    json_reader_read_element (reader, element_index);

    element = json_reader_get_string_value (reader);
    if (element == NULL)
      goto element_error;

    g_ptr_array_index (array, element_index) = g_strdup (element);

    json_reader_end_element (reader);
  }

  json_reader_end_member (reader);

  return TRUE;

element_error:
  {
    json_reader_end_element (reader);
  }
member_error:
  {
    json_reader_end_member (reader);

    g_ptr_array_set_size (array, 0);

    return FALSE;
  }
}

static gboolean
gum_parse_segment (const gchar ** cursor,
                   gint * segment,
                   guint * segment_length)
{
  if (!gum_parse_vlq_value (cursor, &segment[0]))
    return FALSE;

  if (!gum_parse_vlq_value (cursor, &segment[1]))
  {
    *segment_length = 1;
    return TRUE;
  }

  if (!gum_parse_vlq_value (cursor, &segment[2]))
    return FALSE;

  if (!gum_parse_vlq_value (cursor, &segment[3]))
    return FALSE;

  if (gum_parse_vlq_value (cursor, &segment[4]))
    *segment_length = 5;
  else
    *segment_length = 4;
  return TRUE;
}

static const gint8 gum_vlq_character_to_digit[256] =
{
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
  61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
  14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
  46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1
};

static gboolean
gum_parse_vlq_value (const gchar ** cursor,
                     gint * value)
{
  const gchar * c = *cursor;
  guint result = 0, offset = 0;
  gboolean has_continuation, is_positive;

  do
  {
    gint8 digit;
    guint chunk;

    digit = gum_vlq_character_to_digit[(guint8) *c++];
    if (digit == -1)
      return FALSE;

    chunk = digit & 0x1f;
    result |= (chunk << offset);
    offset += 5;

    has_continuation = (digit & (1 << 5)) != 0;
  }
  while (has_continuation);

  *cursor = c;

  is_positive = (result & 1) == 0;
  if (is_positive)
    *value = result >> 1;
  else
    *value = -((gint) (result >> 1));

  return TRUE;
}

gchar *
gum_source_map_try_extract_inline (const gchar * source)
{
  gchar * result = NULL;
  const gchar * line = source;

  while (*line != '\0' && result == NULL)
  {
    const gchar * newline;
    gsize line_len;
    const gchar * p, * base64_tag, * data_start, * data_end;
    gchar * data_encoded;
    gsize decoded_size;
    guchar * decoded;

    newline = strchr (line, '\n');

    line_len = (newline != NULL)
        ? (gsize) (newline - line)
        : strlen (line);

    if (line_len != 0 && line[line_len - 1] == '\r')
      line_len--;

    if (!gum_line_starts_with_source_map_comment (line, line_len))
      goto skip;
    p = line + 4;

    if (!g_str_has_prefix (p, "sourceMappingURL="))
      goto skip;
    p += strlen ("sourceMappingURL=");

    while (*p == ' ' || *p == '\t')
      p++;

    if (!g_str_has_prefix (p, "data:application/json;"))
      goto skip;
    p += strlen ("data:application/json;");

    base64_tag = g_strstr_len (p, line + line_len - p, "base64,");
    if (base64_tag == NULL)
      goto skip;

    data_start = base64_tag + strlen ("base64,");
    data_end = data_start;

    while (data_end < line + line_len &&
        !g_ascii_isspace (*data_end) &&
        *data_end != '\'' && *data_end != '"')
    {
      data_end++;
    }

    if (data_end <= data_start)
      goto skip;

    data_encoded = g_strndup (data_start, data_end - data_start);

    decoded = g_base64_decode (data_encoded, &decoded_size);

    if (decoded != NULL &&
        g_utf8_validate ((const gchar *) decoded, decoded_size, NULL))
    {
      result = g_strndup ((const gchar *) decoded, decoded_size);
    }

    g_free (decoded);
    g_free (data_encoded);

skip:
    if (newline == NULL)
      break;
    line = newline + 1;
  }

  return result;
}

static gboolean
gum_line_starts_with_source_map_comment (const gchar * line,
                                         gsize len)
{
  return len >= 4 &&
      line[0] == '/' && line[1] == '/' &&
      (line[2] == '#' || line[2] == '@') &&
      (line[3] == ' ' || line[3] == '\t');
}
