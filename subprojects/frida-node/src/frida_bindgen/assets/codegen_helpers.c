static gboolean
fdn_is_null (napi_env env,
             napi_value value)
{
  napi_valuetype type;

  napi_typeof (env, value, &type);

  return type == napi_null;
}

static gboolean
fdn_is_undefined_or_null (napi_env env,
                          napi_value value)
{
  napi_valuetype type;

  napi_typeof (env, value, &type);

  return type == napi_undefined || type == napi_null;
}

static gboolean
fdn_is_function (napi_env env,
                 napi_value value)
{
  napi_valuetype type;

  napi_typeof (env, value, &type);

  return type == napi_function;
}

static gboolean
fdn_boolean_from_value (napi_env env,
                        napi_value value,
                        gboolean * b)
{
  bool napi_b;

  if (napi_get_value_bool (env, value, &napi_b) != napi_ok)
    goto invalid_argument;

  *b = napi_b;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected a boolean");
    return FALSE;
  }
}

static napi_value
fdn_boolean_to_value (napi_env env,
                      gboolean b)
{
  napi_value result;
  napi_get_boolean (env, b, &result);
  return result;
}

static gboolean
fdn_size_from_value (napi_env env,
                     napi_value value,
                     gsize * s)
{
  double d;

  if (napi_get_value_double (env, value, &d) != napi_ok)
    goto invalid_argument;

  if (d < 0 || d > G_MAXSIZE)
    goto invalid_argument;

  *s = d;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an unsigned integer");
    return FALSE;
  }
}

static napi_value
fdn_size_to_value (napi_env env,
                   gsize s)
{
  return fdn_ssize_to_value (env, s);
}

static napi_value
fdn_ssize_to_value (napi_env env,
                    gssize s)
{
  napi_value result;
  napi_create_int64 (env, s, &result);
  return result;
}

static gboolean
fdn_int_from_value (napi_env env,
                    napi_value value,
                    gint * i)
{
  int32_t napi_i;

  if (napi_get_value_int32 (env, value, &napi_i) != napi_ok)
    goto invalid_argument;

  *i = napi_i;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an integer");
    return FALSE;
  }
}

static napi_value
fdn_int_to_value (napi_env env,
                  gint i)
{
  napi_value result;
  napi_create_int32 (env, i, &result);
  return result;
}

static gboolean
fdn_uint_from_value (napi_env env,
                     napi_value value,
                     guint * u)
{
  uint32_t napi_u;

  if (napi_get_value_uint32 (env, value, &napi_u) != napi_ok)
    goto invalid_argument;

  *u = napi_u;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an unsigned integer");
    return FALSE;
  }
}

static napi_value
fdn_uint_to_value (napi_env env,
                   guint u)
{
  napi_value result;
  napi_create_uint32 (env, u, &result);
  return result;
}

static napi_value
fdn_int8_to_value (napi_env env,
                   gint8 i)
{
  napi_value result;
  napi_create_int32 (env, i, &result);
  return result;
}

static napi_value
fdn_int16_to_value (napi_env env,
                    gint16 i)
{
  napi_value result;
  napi_create_int32 (env, i, &result);
  return result;
}

static gboolean
fdn_uint16_from_value (napi_env env,
                       napi_value value,
                       guint16 * u)
{
  uint32_t napi_u;

  if (napi_get_value_uint32 (env, value, &napi_u) != napi_ok)
    goto invalid_argument;

  if (napi_u > G_MAXUINT16)
    goto invalid_argument;

  *u = napi_u;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an unsigned 16-bit integer");
    return FALSE;
  }
}

static napi_value
fdn_uint16_to_value (napi_env env,
                     guint16 u)
{
  return fdn_uint32_to_value (env, u);
}

static napi_value
fdn_int32_to_value (napi_env env,
                    gint32 i)
{
  napi_value result;
  napi_create_int32 (env, i, &result);
  return result;
}

static napi_value
fdn_uint32_to_value (napi_env env,
                     guint32 u)
{
  napi_value result;
  napi_create_uint32 (env, u, &result);
  return result;
}

static gboolean
fdn_int64_from_value (napi_env env,
                      napi_value value,
                      gint64 * i)
{
  int64_t napi_i;

  if (napi_get_value_int64 (env, value, &napi_i) != napi_ok)
    goto invalid_argument;

  *i = napi_i;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an integer");
    return FALSE;
  }
}

static napi_value
fdn_int64_to_value (napi_env env,
                    gint64 i)
{
  napi_value result;
  napi_create_bigint_int64 (env, i, &result);
  return result;
}

static gboolean
fdn_uint64_from_value (napi_env env,
                       napi_value value,
                       guint64 * u)
{
  bool lossless;

  if (napi_get_value_bigint_uint64 (env, value, u, &lossless) != napi_ok)
    goto invalid_argument;

  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an unsigned BigInt");
    return FALSE;
  }
}

static napi_value
fdn_uint64_to_value (napi_env env,
                     guint64 u)
{
  napi_value result;
  napi_create_bigint_uint64 (env, u, &result);
  return result;
}

static gboolean
fdn_ulong_from_value (napi_env env,
                      napi_value value,
                      gulong * u)
{
  double d;

  if (napi_get_value_double (env, value, &d) != napi_ok)
    goto invalid_argument;

  if (d < 0 || d > G_MAXULONG)
    goto invalid_argument;

  *u = d;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an unsigned integer");
    return FALSE;
  }
}

static gboolean
fdn_double_from_value (napi_env env,
                       napi_value value,
                       gdouble * d)
{
  if (napi_get_value_double (env, value, d) != napi_ok)
    goto invalid_argument;

  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected a number");
    return FALSE;
  }
}

static napi_value
fdn_double_to_value (napi_env env,
                     gdouble d)
{
  napi_value result;
  napi_create_double (env, d, &result);
  return result;
}

static gboolean
fdn_enum_from_value (napi_env env,
                     GType enum_type,
                     napi_value value,
                     gint * e)
{
  gboolean success = FALSE;
  gchar * nick;
  GEnumClass * enum_class;
  guint i;

  if (!fdn_utf8_from_value (env, value, &nick))
    return FALSE;

  enum_class = G_ENUM_CLASS (g_type_class_ref (enum_type));

  for (i = 0; i != enum_class->n_values; i++)
  {
    GEnumValue * enum_value = &enum_class->values[i];
    if (strcmp (enum_value->value_nick, nick) == 0)
    {
      *e = enum_value->value;
      success = TRUE;
      break;
    }
  }

  g_type_class_unref (enum_class);

  g_free (nick);

  if (!success)
    napi_throw_error (env, NULL, "invalid enumeration value");

  return success;
}

static napi_value
fdn_enum_to_value (napi_env env,
                   GType enum_type,
                   gint e)
{
  napi_value result;
  GEnumClass * enum_class;
  GEnumValue * enum_value;

  enum_class = G_ENUM_CLASS (g_type_class_ref (enum_type));

  enum_value = g_enum_get_value (enum_class, e);
  g_assert (enum_value != NULL);

  result = fdn_utf8_to_value (env, enum_value->value_nick);

  g_type_class_unref (enum_class);

  return result;
}

static gboolean
fdn_utf8_from_value (napi_env env,
                     napi_value value,
                     gchar ** str)
{
  gchar * result = NULL;
  size_t length;

  if (napi_get_value_string_utf8 (env, value, NULL, 0, &length) != napi_ok)
    goto invalid_argument;

  result = g_malloc (length + 1);
  if (napi_get_value_string_utf8 (env, value, result, length + 1, &length) != napi_ok)
    goto invalid_argument;

  *str = result;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected a string");
    g_free (result);
    return FALSE;
  }
}

static napi_value
fdn_utf8_to_value (napi_env env,
                   const gchar * str)
{
  napi_value result;

  if (str == NULL)
  {
    napi_get_null (env, &result);
    return result;
  }

  napi_create_string_utf8 (env, str, NAPI_AUTO_LENGTH, &result);
  return result;
}

static gboolean
fdn_strv_from_value (napi_env env,
                     napi_value value,
                     gchar *** strv)
{
  uint32_t length, i;
  gchar ** vector = NULL;

  if (napi_get_array_length (env, value, &length) != napi_ok)
    goto invalid_argument;

  vector = g_new0 (gchar *, length + 1);

  for (i = 0; i != length; i++)
  {
    napi_value js_str;

    if (napi_get_element (env, value, i, &js_str) != napi_ok)
      goto invalid_argument;

    if (!fdn_utf8_from_value (env, js_str, &vector[i]))
      goto invalid_argument;
  }

  *strv = vector;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected an array of strings");
    g_strfreev (vector);
    return FALSE;
  }
}

static napi_value
fdn_strv_to_value (napi_env env,
                   gchar ** strv)
{
  napi_value result;
  uint32_t length, i;

  length = g_strv_length (strv);

  napi_create_array_with_length (env, length, &result);

  for (i = 0; i != length; i++)
    napi_set_element (env, result, i, fdn_utf8_to_value (env, strv[i]));

  return result;
}

static napi_value
fdn_buffer_to_value (napi_env env,
                     const guint8 * data,
                     gsize size)
{
  napi_value result;
  napi_create_buffer_copy (env, size, data, NULL, &result);
  return result;
}

static gboolean
fdn_bytes_from_value (napi_env env,
                      napi_value value,
                      GBytes ** bytes)
{
  void * data;
  size_t size;

  if (napi_get_buffer_info (env, value, &data, &size) != napi_ok)
    goto invalid_argument;

  *bytes = g_bytes_new (data, size);
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected a buffer");
    return FALSE;
  }
}

static napi_value
fdn_bytes_to_value (napi_env env,
                    GBytes * bytes)
{
  const guint8 * data;
  gsize size;

  data = g_bytes_get_data (bytes, &size);

  return fdn_buffer_to_value (env, data, size);
}

static gboolean
fdn_vardict_from_value (napi_env env,
                        napi_value value,
                        GHashTable ** vardict)
{
  napi_value keys;
  uint32_t length, i;
  GHashTable * dict = NULL;
  gchar * key = NULL;

  if (napi_get_property_names (env, value, &keys) != napi_ok)
    goto invalid_argument;
  if (napi_get_array_length (env, keys, &length) != napi_ok)
    goto propagate_error;

  dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

  for (i = 0; i != length; i++)
  {
    napi_value js_key, js_val;
    GVariant * val;

    if (napi_get_element (env, keys, i, &js_key) != napi_ok)
      goto propagate_error;
    if (!fdn_utf8_from_value (env, js_key, &key))
      goto invalid_argument;

    if (napi_get_property (env, value, js_key, &js_val) != napi_ok)
      goto propagate_error;
    if (!fdn_variant_from_value (env, js_val, &val))
      goto propagate_error;

    g_hash_table_insert (dict, g_steal_pointer (&key), val);
  }

  *vardict = dict;
  return TRUE;

invalid_argument:
  {
    napi_throw_error (env, NULL, "expected a vardict");
    goto propagate_error;
  }
propagate_error:
  {
    g_free (key);
    g_clear_pointer (&dict, g_hash_table_unref);
    return FALSE;
  }
}

static napi_value
fdn_vardict_to_value (napi_env env,
                      GHashTable * vardict)
{
  napi_value result;
  GHashTableIter iter;
  gpointer key, value;

  napi_create_object (env, &result);

  g_hash_table_iter_init (&iter, vardict);
  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    napi_value js_key, js_value;

    js_key = fdn_utf8_to_value (env, key);
    js_value = fdn_variant_to_value (env, value);

    napi_set_property (env, result, js_key, js_value);
  }

  return result;
}

static gboolean
fdn_variant_from_value (napi_env env,
                        napi_value value,
                        GVariant ** variant)
{
  napi_valuetype type;

  napi_typeof (env, value, &type);

  switch (type)
  {
    case napi_boolean:
    {
      gboolean b;

      if (!fdn_boolean_from_value (env, value, &b))
        return FALSE;

      *variant = g_variant_ref_sink (g_variant_new_boolean (b));
      return TRUE;
    }
    case napi_number:
    {
      gint64 i;

      if (!fdn_int64_from_value (env, value, &i))
        return FALSE;

      *variant = g_variant_ref_sink (g_variant_new_int64 (i));
      return TRUE;
    }
    case napi_string:
    {
      gchar * str;

      if (!fdn_utf8_from_value (env, value, &str))
        return FALSE;

      *variant = g_variant_ref_sink (g_variant_new_take_string (str));
      return TRUE;
    }
    case napi_object:
    {
      bool is_buffer, is_array;
      GVariantBuilder builder;
      napi_value keys;
      uint32_t length, i;

      if (napi_is_buffer (env, value, &is_buffer) != napi_ok)
        return FALSE;
      if (is_buffer)
      {
        void * data;
        size_t size;
        gpointer copy;

        if (napi_get_buffer_info (env, value, &data, &size) != napi_ok)
          return FALSE;

        copy = g_memdup2 (data, size);
        *variant = g_variant_ref_sink (g_variant_new_from_data (G_VARIANT_TYPE_BYTESTRING, copy, size, TRUE, g_free, copy));
        return TRUE;
      }

      if (napi_is_array (env, value, &is_array) != napi_ok)
        return FALSE;
      if (is_array)
      {
        uint32_t length;

        if (napi_get_array_length (env, value, &length) != napi_ok)
          return FALSE;

        if (length == 2)
        {
          napi_value first;
          napi_valuetype first_type;

          if (napi_get_element (env, value, 0, &first) != napi_ok)
            return FALSE;

          napi_typeof (env, first, &first_type);

          if (first_type == napi_symbol)
          {
            napi_value second;
            GVariant * val;
            napi_value desc_prop;
            napi_value desc;
            gchar * type;
            GVariant * t[2];

            if (napi_get_element (env, value, 1, &second) != napi_ok)
              return FALSE;

            desc_prop = fdn_utf8_to_value (env, "description");
            napi_get_property (env, first, desc_prop, &desc);
            fdn_utf8_from_value (env, desc, &type);

            if (!fdn_variant_from_value (env, second, &val))
            {
              g_free (type);
              return FALSE;
            }

            t[0] = g_variant_new_take_string (type);
            t[1] = val;

            *variant = g_variant_ref_sink (g_variant_new_tuple (t, G_N_ELEMENTS (t)));

            g_variant_unref (val);

            return TRUE;
          }
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

        for (i = 0; i != length; i++)
        {
          napi_value element;
          GVariant * v;

          if (napi_get_element (env, value, i, &element) != napi_ok)
          {
            g_variant_builder_clear (&builder);
            return FALSE;
          }

          if (!fdn_variant_from_value (env, element, &v))
          {
            g_variant_builder_clear (&builder);
            return FALSE;
          }

          g_variant_builder_add (&builder, "v", v);

          g_variant_unref (v);
        }

        *variant = g_variant_ref_sink (g_variant_builder_end (&builder));
        return TRUE;
      }

      g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

      if (napi_get_property_names (env, value, &keys) != napi_ok)
        return FALSE;

      if (napi_get_array_length (env, keys, &length) != napi_ok)
        return FALSE;

      for (i = 0; i != length; i++)
      {
        napi_value key;
        gchar * key_str;
        napi_value val;
        GVariant * v;

        if (napi_get_element (env, keys, i, &key) != napi_ok)
          return FALSE;

        if (!fdn_utf8_from_value (env, key, &key_str))
          return FALSE;

        if (napi_get_property (env, value, key, &val) != napi_ok)
        {
          g_free (key_str);
          return FALSE;
        }

        if (!fdn_variant_from_value (env, val, &v))
        {
          g_free (key_str);
          return FALSE;
        }

        g_variant_builder_add (&builder, "{sv}", key_str, v);

        g_variant_unref (v);
        g_free (key_str);
      }

      *variant = g_variant_ref_sink (g_variant_builder_end (&builder));
      return TRUE;
    }
    case napi_bigint:
    {
      guint64 u;

      if (!fdn_uint64_from_value (env, value, &u))
        return FALSE;

      *variant = g_variant_ref_sink (g_variant_new_uint64 (u));
      return TRUE;
    }
    default:
      break;
  }

  napi_throw_type_error (env, NULL, "expected value serializable to GVariant");
  return FALSE;
}

static napi_value
fdn_variant_to_value (napi_env env,
                      GVariant * variant)
{
  napi_value result;

  switch (g_variant_classify (variant))
  {
    case G_VARIANT_CLASS_STRING:
    {
      const gchar * str = g_variant_get_string (variant, NULL);
      return fdn_utf8_to_value (env, str);
    }
    case G_VARIANT_CLASS_BYTE:
      return fdn_int8_to_value (env, g_variant_get_byte (variant));
    case G_VARIANT_CLASS_INT16:
      return fdn_int16_to_value (env, g_variant_get_int16 (variant));
    case G_VARIANT_CLASS_UINT16:
      return fdn_uint16_to_value (env, g_variant_get_uint16 (variant));
    case G_VARIANT_CLASS_INT32:
      return fdn_int32_to_value (env, g_variant_get_int32 (variant));
    case G_VARIANT_CLASS_UINT32:
      return fdn_uint32_to_value (env, g_variant_get_uint32 (variant));
    case G_VARIANT_CLASS_INT64:
      return fdn_int64_to_value (env, g_variant_get_int64 (variant));
    case G_VARIANT_CLASS_UINT64:
      return fdn_uint64_to_value (env, g_variant_get_uint64 (variant));
    case G_VARIANT_CLASS_DOUBLE:
      return fdn_double_to_value (env, g_variant_get_double (variant));
    case G_VARIANT_CLASS_BOOLEAN:
      return fdn_boolean_to_value (env, g_variant_get_boolean (variant));
    case G_VARIANT_CLASS_ARRAY:
      if (g_variant_is_of_type (variant, G_VARIANT_TYPE ("ay")))
      {
        gsize size;
        g_variant_get_fixed_array (variant, &size, sizeof (guint8));
        return fdn_buffer_to_value (env, g_variant_get_data (variant), size);
      }

      if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT))
      {
        napi_value dict;
        GVariantIter iter;
        gchar * key;
        GVariant * value;

        napi_create_object (env, &dict);

        g_variant_iter_init (&iter, variant);
        while (g_variant_iter_next (&iter, "{sv}", &key, &value))
        {
          napi_value js_key, js_value;

          js_key = fdn_utf8_to_value (env, key);
          js_value = fdn_variant_to_value (env, value);

          napi_set_property (env, dict, js_key, js_value);

          g_variant_unref (value);
          g_free (key);
        }

        return dict;
      }

      if (g_variant_is_of_type (variant, G_VARIANT_TYPE_ARRAY))
      {
        napi_value array;
        GVariantIter iter;
        uint32_t i;
        GVariant * child;

        napi_create_array (env, &array);

        g_variant_iter_init (&iter, variant);
        i = 0;
        while ((child = g_variant_iter_next_value (&iter)) != NULL)
        {
          napi_value element = fdn_variant_to_value (env, child);
          napi_set_element (env, array, i++, element);
          g_variant_unref (child);
        }

        return array;
      }

      break;
    case G_VARIANT_CLASS_TUPLE:
    {
      napi_value array;
      GVariantIter iter;
      uint32_t i;
      GVariant * child;

      if (g_variant_n_children (variant) == 0)
      {
        napi_get_undefined (env, &result);
        return result;
      }

      napi_create_array (env, &array);

      g_variant_iter_init (&iter, variant);
      i = 0;
      while ((child = g_variant_iter_next_value (&iter)) != NULL)
      {
        napi_value element = fdn_variant_to_value (env, child);
        napi_set_element (env, array, i++, element);
        g_variant_unref (child);
      }

      return array;
    }
    case G_VARIANT_CLASS_VARIANT:
    {
      GVariant * inner;

      inner = g_variant_get_variant (variant);
      result = fdn_variant_to_value (env, inner);
      g_variant_unref (inner);

      return result;
    }
    default:
      break;
  }

  napi_get_null (env, &result);
  return result;
}

static gboolean
fdn_gvalue_from_value (napi_env env,
                       GType type,
                       napi_value js_value,
                       GValue * value)
{
  g_value_init (value, type);

  switch (type)
  {
    case G_TYPE_BOOLEAN:
    {
      gboolean b;

      if (!fdn_boolean_from_value (env, js_value, &b))
        return FALSE;
      g_value_set_boolean (value, b);

      break;
    }
    case G_TYPE_INT:
    {
      gint i;

      if (!fdn_int_from_value (env, js_value, &i))
        return FALSE;
      g_value_set_int (value, i);

      break;
    }
    case G_TYPE_UINT:
    {
      guint u;

      if (!fdn_uint_from_value (env, js_value, &u))
        return FALSE;
      g_value_set_uint (value, u);

      break;
    }
    case G_TYPE_INT64:
    {
      gint64 i;

      if (!fdn_int64_from_value (env, js_value, &i))
        return FALSE;
      g_value_set_int64 (value, i);

      break;
    }
    case G_TYPE_UINT64:
    {
      guint64 u;

      if (!fdn_uint64_from_value (env, js_value, &u))
        return FALSE;
      g_value_set_uint64 (value, u);

      break;
    }
    case G_TYPE_FLOAT:
    {
      gdouble d;

      if (!fdn_double_from_value (env, js_value, &d))
        return FALSE;
      g_value_set_float (value, d);

      break;
    }
    case G_TYPE_DOUBLE:
    {
      gdouble d;

      if (!fdn_double_from_value (env, js_value, &d))
        return FALSE;
      g_value_set_double (value, d);

      break;
    }
    case G_TYPE_STRING:
    {
      gchar * str;

      if (!fdn_utf8_from_value (env, js_value, &str))
        return FALSE;
      g_value_take_string (value, str);

      break;
    }
    default:
    {
      gchar * msg;

      if (G_TYPE_IS_ENUM (type))
      {
        gint e;

        if (!fdn_enum_from_value (env, type, js_value, &e))
          return FALSE;
        g_value_set_enum (value, e);

        return TRUE;
      }

      if (type == G_TYPE_STRV)
      {
        gchar ** strv;

        if (!fdn_strv_from_value (env, js_value, &strv))
          return FALSE;
        g_value_take_boxed (value, strv);

        return TRUE;
      }

      if (type == G_TYPE_BYTES)
      {
        GBytes * bytes;

        if (!fdn_bytes_from_value (env, js_value, &bytes))
          return FALSE;
        g_value_take_boxed (value, bytes);

        return TRUE;
      }

      if (type == G_TYPE_HASH_TABLE)
      {
        GHashTable * vardict;

        if (!fdn_vardict_from_value (env, js_value, &vardict))
          return FALSE;
        g_value_take_boxed (value, vardict);

        return TRUE;
      }

      if (type == G_TYPE_TLS_CERTIFICATE)
      {
        GTlsCertificate * certificate;

        if (!fdn_tls_certificate_from_value (env, js_value, &certificate))
          return FALSE;
        g_value_take_object (value, certificate);

        return TRUE;
      }

      if (G_TYPE_IS_OBJECT (type))
      {
        GObject * object;

        if (!fdn_object_unwrap (env, js_value, type, &object))
          return FALSE;
        g_value_take_object (value, object);

        return TRUE;
      }

      msg = g_strdup_printf ("unsupported property type: %s", g_type_name (type));
      napi_throw_type_error (env, NULL, msg);
      g_free (msg);

      return FALSE;
    }
  }

  return TRUE;
}

static napi_value
fdn_gvalue_to_value (napi_env env,
                     GValue * value)
{
  GType gtype;

  gtype = G_VALUE_TYPE (value);

  switch (gtype)
  {
    case G_TYPE_BOOLEAN:
      return fdn_boolean_to_value (env, g_value_get_boolean (value));
    case G_TYPE_INT:
      return fdn_int_to_value (env, g_value_get_int (value));
    case G_TYPE_UINT:
      return fdn_uint_to_value (env, g_value_get_uint (value));
    case G_TYPE_FLOAT:
      return fdn_double_to_value (env, g_value_get_float (value));
    case G_TYPE_DOUBLE:
      return fdn_double_to_value (env, g_value_get_double (value));
    case G_TYPE_STRING:
    {
      const gchar * str;

      str = g_value_get_string (value);
      if (str == NULL)
      {
        napi_value result;
        napi_get_null (env, &result);
        return result;
      }

      return fdn_utf8_to_value (env, str);
    }
    case G_TYPE_VARIANT:
      return fdn_variant_to_value (env, g_value_get_variant (value));
    default:
    {
      napi_value result;

      if (G_TYPE_IS_ENUM (gtype))
        return fdn_enum_to_value (env, gtype, g_value_get_enum (value));

      if (gtype == G_TYPE_BYTES)
      {
        GBytes * bytes = g_value_get_boxed (value);
        if (bytes != NULL)
        {
          return fdn_bytes_to_value (env, bytes);
        }
        else
        {
          napi_get_null (env, &result);
          return result;
        }
      }

      if (G_TYPE_IS_OBJECT (gtype))
        result = fdn_object_subclass_to_value (env, g_value_get_object (value));
      else
        napi_get_null (env, &result);

      return result;
    }
  }
}

static gboolean
fdn_error_from_value (napi_env env,
                      napi_value value,
                      GError ** error)
{
  napi_value js_message;
  gchar * raw_message;
  GString * message;

  if (napi_get_named_property (env, value, "message", &js_message) != napi_ok)
    return FALSE;

  if (!fdn_utf8_from_value (env, js_message, &raw_message))
    return FALSE;

  message = g_string_new ("");
  g_string_append_unichar (message, g_unichar_toupper (g_utf8_get_char (raw_message)));
  g_string_append (message, g_utf8_offset_to_pointer (raw_message, 1));

  *error = g_error_new_literal (FRIDA_ERROR, FRIDA_ERROR_INVALID_ARGUMENT, message->str);

  g_free (raw_message);
  g_string_free (message, TRUE);

  return TRUE;
}

static napi_value
fdn_error_to_value (napi_env env,
                    GError * error)
{
  napi_value result;
  napi_create_error (env, NULL, fdn_utf8_to_value (env, error->message), &result);
  return result;
}

static gboolean
fdn_file_from_value (napi_env env,
                     napi_value value,
                     GFile ** file)
{
  gchar * path;

  if (!fdn_utf8_from_value (env, value, &path))
    return FALSE;
  *file = g_file_new_for_path (path);
  g_free (path);

  return TRUE;
}

static napi_value
fdn_file_to_value (napi_env env,
                   GFile * file)
{
  napi_value result;
  gchar * path;

  path = g_file_get_path (file);
  result = fdn_utf8_to_value (env, path);
  g_free (path);

  return result;
}

static gboolean
fdn_tls_certificate_from_value (napi_env env,
                                napi_value value,
                                GTlsCertificate ** certificate)
{
  gchar * str;
  GError * error = NULL;

  if (!fdn_utf8_from_value (env, value, &str))
    return FALSE;

  if (strchr (str, '\n') != NULL)
    *certificate = g_tls_certificate_new_from_pem (str, -1, &error);
  else
    *certificate = g_tls_certificate_new_from_file (str, &error);

  g_free (str);

  if (error != NULL)
    goto invalid_argument;
  return TRUE;

invalid_argument:
  {
    napi_throw (env, fdn_error_to_value (env, error));
    g_error_free (error);
    return FALSE;
  }
}

static napi_value
fdn_tls_certificate_to_value (napi_env env,
                              GTlsCertificate * certificate)
{
  napi_value result;
  gchar * pem;

  g_object_get (certificate, "certificate-pem", &pem, NULL);
  result = fdn_utf8_to_value (env, pem);
  g_free (pem);

  return result;
}

static gboolean
fdn_object_apply_properties (napi_env env,
                             GObject * object,
                             napi_value value,
                             const gchar * const * skipped_keys)
{
  gboolean success = FALSE;
  napi_valuetype value_type;
  napi_value keys;
  uint32_t n_keys, i;
  GObjectClass * object_class;
  gchar * gobject_property_name = NULL;

  if (napi_typeof (env, value, &value_type) != napi_ok || value_type != napi_object)
    goto expected_an_object;

  if (napi_get_property_names (env, value, &keys) != napi_ok)
    return FALSE;

  if (napi_get_array_length (env, keys, &n_keys) != napi_ok)
    return FALSE;

  object_class = G_OBJECT_GET_CLASS (object);

  for (i = 0; i != n_keys; i++)
  {
    napi_value js_key, js_value;
    gchar * property_name;
    GParamSpec * pspec;
    GValue property_value = G_VALUE_INIT;

    if (napi_get_element (env, keys, i, &js_key) != napi_ok)
      goto beach;

    if (!fdn_utf8_from_value (env, js_key, &property_name))
      goto beach;

    if (skipped_keys != NULL && g_strv_contains ((const gchar * const *) skipped_keys, property_name))
    {
      g_free (property_name);
      continue;
    }

    gobject_property_name = fdn_camel_case_to_kebab_case (property_name);
    g_free (property_name);

    pspec = g_object_class_find_property (object_class, gobject_property_name);
    if (pspec == NULL || (pspec->flags & G_PARAM_WRITABLE) == 0)
    {
      g_clear_pointer (&gobject_property_name, g_free);
      continue;
    }

    if (napi_get_property (env, value, js_key, &js_value) != napi_ok)
      goto beach;

    if (!fdn_gvalue_from_value (env, pspec->value_type, js_value, &property_value))
      goto beach;

    g_object_set_property (object, gobject_property_name, &property_value);

    g_value_unset (&property_value);
    g_clear_pointer (&gobject_property_name, g_free);
  }

  success = TRUE;
  goto beach;

expected_an_object:
  {
    napi_throw_type_error (env, NULL, "expected an object");
    return FALSE;
  }
beach:
  {
    g_free (gobject_property_name);

    return success;
  }
}

static gboolean
fdn_options_from_value (napi_env env,
                        GType object_type,
                        napi_value value,
                        gpointer * options)
{
  gboolean success = FALSE;
  napi_valuetype value_type;
  napi_value keys;
  uint32_t n_keys;
  guint n_properties = 0;
  const char ** property_names = NULL;
  GValue * property_values = NULL;
  GObjectClass * object_class = NULL;
  uint32_t i;
  gchar * gobject_property_name = NULL;

  if (napi_typeof (env, value, &value_type) != napi_ok || value_type != napi_object)
    goto expected_an_object;

  if (napi_get_property_names (env, value, &keys) != napi_ok)
    goto beach;

  if (napi_get_array_length (env, keys, &n_keys) != napi_ok)
    goto beach;

  property_names = g_newa (const char *, n_keys);
  property_values = g_newa (GValue, n_keys);

  object_class = G_OBJECT_CLASS (g_type_class_ref (object_type));

  for (i = 0; i != n_keys; i++)
  {
    napi_value js_key, js_value;
    gchar * property_name;
    GParamSpec * pspec;

    if (napi_get_element (env, keys, i, &js_key) != napi_ok)
      goto beach;

    if (!fdn_utf8_from_value (env, js_key, &property_name))
      goto beach;

    gobject_property_name = fdn_camel_case_to_kebab_case (property_name);
    g_free (property_name);

    pspec = g_object_class_find_property (object_class, gobject_property_name);
    if (pspec == NULL)
    {
      g_free (gobject_property_name);
      gobject_property_name = NULL;
      continue;
    }

    if (napi_get_property (env, value, js_key, &js_value) != napi_ok)
      goto beach;

    if (!fdn_gvalue_from_value (env, pspec->value_type, js_value, &property_values[n_properties]))
      goto beach;

    property_names[n_properties] = g_steal_pointer (&gobject_property_name);
    n_properties++;
  }

  *options = g_object_new_with_properties (object_type, n_properties, property_names, property_values);

  success = TRUE;
  goto beach;

expected_an_object:
  {
    napi_throw_type_error (env, NULL, "expected an object");
    goto beach;
  }
beach:
  {
    g_free (gobject_property_name);

    for (i = 0; i != n_properties; i++)
    {
      g_free ((gchar *) property_names[i]);
      g_value_unset (&property_values[i]);
    }

    g_clear_pointer (&object_class, g_type_class_unref);

    return success;
  }
}

static napi_value
fdn_object_subclass_to_value (napi_env env,
                              GObject * object)
{
  napi_value result;
  napi_ref ctor;

  if (object == NULL)
  {
    napi_get_null (env, &result);
    return result;
  }

  ctor = g_hash_table_lookup (fdn_constructors, GSIZE_TO_POINTER (G_OBJECT_TYPE (object)));
  if (ctor == NULL)
    goto unsupported_type;

  return fdn_object_new (env, object, ctor);

unsupported_type:
  {
    napi_get_null (env, &result);
    return result;
  }
}

static napi_value
fdn_object_new (napi_env env,
                GObject * handle,
                napi_ref constructor)
{
  napi_value result, ctor, handle_wrapper;
  napi_ref wrapper_ref;

  if (handle == NULL)
  {
    napi_get_null (env, &result);
    return result;
  }

  wrapper_ref = g_object_get_data (handle, "fdn-wrapper");
  if (wrapper_ref != NULL)
  {
    if (napi_get_reference_value (env, wrapper_ref, &result) == napi_ok && result != NULL)
      return result;
  }

  napi_get_reference_value (env, constructor, &ctor);

  napi_create_external (env, handle, NULL, NULL, &handle_wrapper);
  napi_type_tag_object (env, handle_wrapper, &fdn_handle_wrapper_type_tag);

  napi_new_instance (env, ctor, 1, &handle_wrapper, &result);

  return result;
}

static gboolean
fdn_object_wrap (napi_env env,
                 napi_value wrapper,
                 GObject * handle,
                 napi_finalize finalizer)
{
  napi_ref ref;

  if (napi_type_tag_object (env, wrapper, &fdn_object_type_tag) != napi_ok)
    return FALSE;

  if (napi_wrap (env, wrapper, handle, NULL, NULL, NULL) != napi_ok)
    return FALSE;

  if (napi_add_finalizer (env, wrapper, handle, finalizer, NULL, NULL) != napi_ok)
    return FALSE;

  napi_create_reference (env, wrapper, 0, &ref);
  g_object_set_data (handle, "fdn-wrapper", ref);

  return TRUE;
}

static gboolean
fdn_object_unwrap (napi_env env,
                   napi_value wrapper,
                   GType expected_type,
                   GObject ** handle)
{
  bool is_instance;
  GObject * obj;

  if (napi_check_object_type_tag (env, wrapper, &fdn_object_type_tag, &is_instance) != napi_ok || !is_instance)
    goto invalid_tag;

  if (napi_unwrap (env, wrapper, (void **) &obj) != napi_ok)
    goto invalid_tag;

  if (!g_type_is_a (G_OBJECT_TYPE (obj), expected_type))
    goto invalid_type;

  *handle = g_object_ref (obj);
  return TRUE;

invalid_tag:
  {
    gchar * msg;

    msg = g_strdup_printf ("expected an instance of %s", g_type_name (expected_type));
    napi_throw_type_error (env, NULL, msg);
    g_free (msg);

    return FALSE;
  }
invalid_type:
  {
    gchar * msg;

    msg = g_strdup_printf ("expected an instance of %s, got a %s",
        g_type_name (expected_type),
        g_type_name (G_OBJECT_TYPE (obj)));
    napi_throw_type_error (env, NULL, msg);
    g_free (msg);

    return FALSE;
  }
}

static void
fdn_object_finalize (napi_env env,
                     void * finalize_data,
                     void * finalize_hint)
{
  GObject * handle = G_OBJECT (finalize_data);

  napi_delete_reference (env, g_object_steal_data (handle, "fdn-wrapper"));

  g_object_unref (handle);
}

static napi_value
fdn_object_get_signal (napi_env env,
                       napi_callback_info info,
                       const gchar * name,
                       const gchar * js_storage_name,
                       FdnSignalBehavior behavior)
{
  napi_value result, jsthis, js_storage_name_value;
  napi_valuetype type;

  if (napi_get_cb_info (env, info, NULL, NULL, &jsthis, NULL) != napi_ok)
    return NULL;

  js_storage_name_value = fdn_utf8_to_value (env, js_storage_name);

  if (napi_get_property (env, jsthis, js_storage_name_value, &result) != napi_ok)
    return NULL;

  if (napi_typeof (env, result, &type) != napi_ok)
    return NULL;

  if (type == napi_undefined)
  {{
    GObject * handle;

    if (napi_unwrap (env, jsthis, (void **) &handle) != napi_ok)
      return NULL;

    result = fdn_signal_new (env, handle, name, behavior);
    napi_set_property (env, jsthis, js_storage_name_value, result);
  }}

  return result;
}

static napi_value
fdn_signal_new (napi_env env,
                GObject * handle,
                const gchar * name,
                FdnSignalBehavior behavior)
{
  napi_value result, constructor, handle_wrapper;
  napi_value args[3];

  napi_get_reference_value (env, fdn_signal_constructor, &constructor);

  napi_create_external (env, handle, NULL, NULL, &handle_wrapper);
  napi_type_tag_object (env, handle_wrapper, &fdn_handle_wrapper_type_tag);

  args[0] = handle_wrapper;
  args[1] = fdn_utf8_to_value (env, name);
  args[2] = fdn_int_to_value (env, behavior);

  napi_new_instance (env, constructor, G_N_ELEMENTS (args), args, &result);

  return result;
}

static void
fdn_signal_register (napi_env env,
                     napi_value exports)
{
  napi_property_descriptor properties[] =
  {
    { "connect", NULL, fdn_signal_connect, NULL, NULL, NULL, napi_default, NULL },
    { "disconnect", NULL, fdn_signal_disconnect, NULL, NULL, NULL, napi_default, NULL },
  };
  napi_value constructor;

  napi_define_class (env, "Signal", NAPI_AUTO_LENGTH, fdn_signal_construct, NULL, G_N_ELEMENTS (properties), properties, &constructor);
  napi_create_reference (env, constructor, 1, &fdn_signal_constructor);

  napi_set_named_property (env, exports, "Signal", constructor);
}

static napi_value
fdn_signal_construct (napi_env env,
                      napi_callback_info info)
{
  size_t argc = 3;
  napi_value args[3];
  napi_value jsthis;
  GObject * handle;
  bool is_instance;
  gchar * name = NULL;
  FdnSignalBehavior behavior;
  FdnSignal * sig = NULL;

  if (napi_get_cb_info (env, info, &argc, args, &jsthis, NULL) != napi_ok)
    goto propagate_error;

  if (argc != 3)
    goto missing_argument;

  if (napi_check_object_type_tag (env, args[0], &fdn_handle_wrapper_type_tag, &is_instance) != napi_ok || !is_instance)
    goto invalid_handle;

  if (napi_get_value_external (env, args[0], (void **) &handle) != napi_ok)
    goto propagate_error;

  if (!fdn_utf8_from_value (env, args[1], &name))
    goto propagate_error;

  if (!fdn_int_from_value (env, args[2], (gint *) &behavior))
    goto propagate_error;
  if (behavior != FDN_SIGNAL_ALLOW_EXIT && behavior != FDN_SIGNAL_KEEP_ALIVE)
    goto invalid_behavior;

  sig = g_slice_new (FdnSignal);
  sig->handle = g_object_ref (handle);
  sig->id = g_signal_lookup (name, G_OBJECT_TYPE (sig->handle));
  sig->behavior = behavior;
  sig->closures = NULL;
  if (sig->id == 0)
    goto invalid_signal_name;

  if (napi_wrap (env, jsthis, sig, NULL, NULL, NULL) != napi_ok)
    goto propagate_error;

  if (napi_add_finalizer (env, jsthis, sig, fdn_signal_finalize, NULL, NULL) != napi_ok)
    goto propagate_error;

  g_free (name);

  return jsthis;

missing_argument:
  {
    napi_throw_error (env, NULL, "missing argument");
    goto propagate_error;
  }
invalid_handle:
  {
    napi_throw_type_error (env, NULL, "expected an object handle");
    goto propagate_error;
  }
invalid_behavior:
  {
    napi_throw_error (env, NULL, "invalid behavior");
    goto propagate_error;
  }
invalid_signal_name:
  {
    napi_throw_type_error (env, NULL, "bad signal name");
    goto propagate_error;
  }
propagate_error:
  {
    if (sig != NULL)
      fdn_signal_finalize (env, sig, NULL);

    g_free (name);

    return NULL;
  }
}

static void
fdn_signal_finalize (napi_env env,
                     void * finalize_data,
                     void * finalize_hint)
{
  FdnSignal * sig = finalize_data;
  GSList * cur;

  for (cur = sig->closures; cur != NULL; cur = cur->next)
    fdn_signal_disconnect_closure (sig, cur->data);
  g_slist_free (sig->closures);

  g_object_unref (sig->handle);

  g_slice_free (FdnSignal, sig);
}

static napi_value
fdn_signal_connect (napi_env env,
                    napi_callback_info info)
{
  napi_value js_retval;
  FdnSignal * self;
  napi_value js_self, handler;
  FdnSignalClosure * sc;
  GClosure * closure;

  if (!fdn_signal_parse_arguments (env, info, &self, &js_self, &handler))
    return NULL;

  sc = fdn_signal_closure_new (env, self, js_self, handler);

  closure = (GClosure *) sc;
  g_closure_ref (closure);
  g_closure_sink (closure);
  self->closures = g_slist_prepend (self->closures, sc);

  sc->handler_id = g_signal_connect_closure_by_id (self->handle, self->id, 0, closure, TRUE);

  napi_get_undefined (env, &js_retval);

  return js_retval;
}

static napi_value
fdn_signal_disconnect (napi_env env,
                       napi_callback_info info)
{
  napi_value js_retval;
  FdnSignal * self;
  napi_value handler;
  GSList * cur;

  if (!fdn_signal_parse_arguments (env, info, &self, NULL, &handler))
    return NULL;

  for (cur = self->closures; cur != NULL; cur = cur->next)
  {
    FdnSignalClosure * closure = cur->data;
    napi_value candidate_handler;
    bool same_handler;

    napi_get_reference_value (env, closure->handler, &candidate_handler);

    napi_strict_equals (env, candidate_handler, handler, &same_handler);

    if (same_handler)
    {
      fdn_signal_disconnect_closure (self, closure);
      self->closures = g_slist_delete_link (self->closures, cur);
      break;
    }
  }

  napi_get_undefined (env, &js_retval);

  return js_retval;
}

static void
fdn_signal_disconnect_closure (FdnSignal * self,
                               FdnSignalClosure * closure)
{
  g_signal_handler_disconnect (self->handle, closure->handler_id);
  closure->handler_id = 0;

  closure->state = FDN_SIGNAL_CLOSURE_CLOSED;

  g_closure_unref ((GClosure *) closure);
}

static gboolean
fdn_signal_parse_arguments (napi_env env,
                            napi_callback_info info,
                            FdnSignal ** self,
                            napi_value * js_self,
                            napi_value * handler)
{
  size_t argc = 1;
  napi_value jsthis;

  if (napi_get_cb_info (env, info, &argc, handler, &jsthis, NULL) != napi_ok)
    goto propagate_error;

  if (napi_unwrap (env, jsthis, (void **) self) != napi_ok)
    goto propagate_error;

  if (js_self != NULL)
    *js_self = jsthis;

  if (argc != 1)
    goto missing_handler;

  if (!fdn_is_function (env, *handler))
    goto invalid_handler;

  return TRUE;

missing_handler:
  {
    napi_throw_error (env, NULL, "missing argument: handler");
    return FALSE;
  }
invalid_handler:
  {
    napi_throw_error (env, NULL, "expected a function");
    return FALSE;
  }
propagate_error:
  {
    return FALSE;
  }
}

static FdnSignalClosure *
fdn_signal_closure_new (napi_env env,
                        FdnSignal * sig,
                        napi_value js_sig,
                        napi_value handler)
{
  FdnSignalClosure * sc;
  GClosure * closure;

  closure = g_closure_new_simple (sizeof (FdnSignalClosure), NULL);
  g_closure_add_finalize_notifier (closure, NULL, fdn_signal_closure_finalize);
  g_closure_set_marshal (closure, fdn_signal_closure_marshal);

  sc = (FdnSignalClosure *) closure;
  sc->sig = sig;
  napi_create_reference (env, js_sig, 1, &sc->js_sig);
  sc->state = FDN_SIGNAL_CLOSURE_OPEN;
  napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, g_signal_name (sig->id)), 0, 1, NULL, NULL, sc,
      fdn_signal_closure_deliver, &sc->tsfn);

  if (sig->behavior == FDN_SIGNAL_ALLOW_EXIT)
    napi_unref_threadsafe_function (env, sc->tsfn);

  napi_create_reference (env, handler, 1, &sc->handler);

  return sc;
}

static void
fdn_signal_closure_finalize (gpointer data,
                             GClosure * closure)
{
  FdnSignalClosure * self = (FdnSignalClosure *) closure;
  FdnSignalClosureMessage * message;
  FdnSignalClosureMessageDestroy * d;

  if (fdn_in_cleanup)
    return;

  message = g_slice_new (FdnSignalClosureMessage);
  message->type = FDN_SIGNAL_CLOSURE_MESSAGE_DESTROY;

  d = &message->payload.destroy;
  d->js_sig = self->js_sig;
  d->tsfn = self->tsfn;
  d->handler = self->handler;

  napi_call_threadsafe_function (self->tsfn, message, napi_tsfn_blocking);
}

static void
fdn_signal_closure_marshal (GClosure * closure,
                            GValue * return_gvalue,
                            guint n_param_values,
                            const GValue * param_values,
                            gpointer invocation_hint,
                            gpointer marshal_data)
{
  FdnSignalClosure * self = (FdnSignalClosure *) closure;
  FdnSignalClosureMessage * message;
  GArray * args;
  guint i;

  message = g_slice_new (FdnSignalClosureMessage);
  message->type = FDN_SIGNAL_CLOSURE_MESSAGE_MARSHAL;

  g_assert (n_param_values >= 1);
  args = g_array_sized_new (FALSE, FALSE, sizeof (GValue), n_param_values - 1);
  message->payload.marshal.args = args;

  for (i = 1; i != n_param_values; i++)
  {
    GValue val;

    g_value_init (&val, param_values[i].g_type);
    g_value_copy (&param_values[i], &val);
    g_array_append_val (args, val);
  }

  g_closure_ref (closure);
  napi_call_threadsafe_function (self->tsfn, message, napi_tsfn_blocking);
}

static void
fdn_signal_closure_deliver (napi_env env,
                            napi_value js_cb,
                            void * context,
                            void * data)
{
  FdnSignalClosureMessage * message = data;

  switch (message->type)
  {
    case FDN_SIGNAL_CLOSURE_MESSAGE_DESTROY:
    {
      FdnSignalClosureMessageDestroy * d = &message->payload.destroy;
      napi_value js_sig;
      FdnSignal * sig;

      napi_get_reference_value (env, d->js_sig, &js_sig);
      napi_unwrap (env, js_sig, (void **) &sig);

      napi_delete_reference (env, d->handler);
      napi_delete_reference (env, d->js_sig);
      napi_release_threadsafe_function (d->tsfn, napi_tsfn_abort);

      break;
    }
    case FDN_SIGNAL_CLOSURE_MESSAGE_MARSHAL:
    {
      FdnSignalClosure * self = context;
      GArray * args;
      guint i;

      args = message->payload.marshal.args;

      if (self->state == FDN_SIGNAL_CLOSURE_OPEN)
      {
        napi_value * js_args;
        napi_value global, handler, js_result;

        js_args = g_newa (napi_value, args->len);
        for (i = 0; i != args->len; i++)
          js_args[i] = fdn_gvalue_to_value (env, &g_array_index (args, GValue, i));

        napi_get_global (env, &global);
        napi_get_reference_value (env, self->handler, &handler);

        if (napi_call_function (env, global, handler, args->len, js_args, &js_result) == napi_pending_exception)
        {
          napi_value err;

          napi_get_and_clear_last_exception (env, &err);
          napi_fatal_exception (env, err);
        }
      }

      for (i = 0; i != args->len; i++)
        g_value_reset (&g_array_index (args, GValue, i));
      g_array_free (args, TRUE);

      g_closure_unref ((GClosure *) self);

      break;
    }
    default:
      g_assert_not_reached ();
  }

  g_slice_free (FdnSignalClosureMessage, message);
}

static void
fdn_keep_alive_until (napi_env env,
                      napi_value js_object,
                      GObject * handle,
                      FdnIsDestroyedFunc is_destroyed,
                      const gchar * destroy_signal_name)
{
  FdnKeepAliveContext * context;

  context = g_slice_new (FdnKeepAliveContext);
  context->ref_count = 2;
  context->handle = g_object_ref (handle);
  context->signal_handler_id = 0;

  napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, "FridaKeepAlive"), 0, 1, NULL, NULL, context, fdn_keep_alive_on_tsfn_invoke, &context->tsfn);

  napi_add_finalizer (env, js_object, context, fdn_keep_alive_on_finalize, NULL, NULL);

  context->signal_handler_id = g_signal_connect_data (handle, destroy_signal_name, G_CALLBACK (fdn_keep_alive_on_destroy_signal), context,
      fdn_keep_alive_on_destroy_signal_handler_detached, 0);

  if (is_destroyed (handle))
  {
    g_atomic_int_inc (&context->ref_count);
    fdn_keep_alive_schedule_cleanup (context);
  }
}

static void
fdn_keep_alive_on_finalize (napi_env env,
                            void * finalize_data,
                            void * finalize_hint)
{
  FdnKeepAliveContext * context = finalize_data;

  fdn_keep_alive_schedule_cleanup (context);
}

static void
fdn_keep_alive_on_destroy_signal (GObject * handle,
                                  gpointer user_data)
{
  FdnKeepAliveContext * context = user_data;

  g_atomic_int_inc (&context->ref_count);
  fdn_keep_alive_schedule_cleanup (context);
}

static void
fdn_keep_alive_on_destroy_signal_handler_detached (gpointer data,
                                                   GClosure * closure)
{
  FdnKeepAliveContext * context = data;

  fdn_keep_alive_schedule_cleanup (context);
}

static void
fdn_keep_alive_schedule_cleanup (FdnKeepAliveContext * context)
{
  napi_threadsafe_function tsfn;

  if (fdn_in_cleanup)
    return;

  if ((tsfn = g_atomic_pointer_exchange (&context->tsfn, NULL)) != NULL)
    napi_call_threadsafe_function (tsfn, tsfn, napi_tsfn_blocking);
  else
    fdn_keep_alive_context_unref (context);
}

static void
fdn_keep_alive_on_tsfn_invoke (napi_env env,
                               napi_value js_cb,
                               void * context,
                               void * data)
{
  FdnKeepAliveContext * ctx = context;
  napi_threadsafe_function tsfn = data;

  g_signal_handler_disconnect (ctx->handle, ctx->signal_handler_id);
  ctx->signal_handler_id = 0;

  g_object_unref (ctx->handle);
  ctx->handle = NULL;

  napi_release_threadsafe_function (tsfn, napi_tsfn_abort);

  fdn_keep_alive_context_unref (ctx);
}

static void
fdn_keep_alive_context_unref (FdnKeepAliveContext * ctx)
{
  if (g_atomic_int_dec_and_test (&ctx->ref_count))
    g_slice_free (FdnKeepAliveContext, ctx);
}

static void
fdn_inherit_val_val (napi_env env,
                     napi_value sub_ctor,
                     napi_value super_ctor,
                     napi_value object_ctor,
                     napi_value set_proto)
{
  napi_value argv[2], sub_proto, super_proto;

  argv[0] = sub_ctor;
  argv[1] = super_ctor;
  napi_call_function (env, object_ctor, set_proto, G_N_ELEMENTS (argv), argv, NULL);

  napi_get_named_property (env, sub_ctor, "prototype", &sub_proto);
  napi_get_named_property (env, super_ctor, "prototype", &super_proto);
  argv[0] = sub_proto;
  argv[1] = super_proto;
  napi_call_function (env, object_ctor, set_proto, G_N_ELEMENTS (argv), argv, NULL);
}

static void
fdn_inherit_val_ref (napi_env env,
                     napi_value sub_ctor,
                     napi_ref super_ctor,
                     napi_value object_ctor,
                     napi_value set_proto)
{
  napi_value super_ctor_val;

  napi_get_reference_value (env, super_ctor, &super_ctor_val);

  fdn_inherit_val_val (env, sub_ctor, super_ctor_val, object_ctor, set_proto);
}

static void
fdn_inherit_ref_val (napi_env env,
                     napi_ref sub_ctor,
                     napi_value super_ctor,
                     napi_value object_ctor,
                     napi_value set_proto)
{
  napi_value sub_ctor_val;

  napi_get_reference_value (env, sub_ctor, &sub_ctor_val);

  fdn_inherit_val_val (env, sub_ctor_val, super_ctor, object_ctor, set_proto);
}

static void
fdn_inherit_ref_ref (napi_env env,
                     napi_ref sub_ctor,
                     napi_ref super_ctor,
                     napi_value object_ctor,
                     napi_value set_proto)
{
  napi_value sub_ctor_val, super_ctor_val;

  napi_get_reference_value (env, sub_ctor, &sub_ctor_val);
  napi_get_reference_value (env, super_ctor, &super_ctor_val);

  fdn_inherit_val_val (env, sub_ctor_val, super_ctor_val, object_ctor, set_proto);
}

static gchar *
fdn_camel_case_to_kebab_case (const gchar * name)
{
  GString * result;
  const gchar * p;

  result = g_string_new (NULL);

  for (p = name; *p != '\0'; p++)
  {
    if (g_ascii_isupper (*p))
    {
      if (p != name)
        g_string_append_c (result, '-');
      g_string_append_c (result, g_ascii_tolower (*p));
    }
    else
    {
      g_string_append_c (result, *p);
    }
  }

  return g_string_free (result, FALSE);
}
