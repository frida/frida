static void
PyGObject_class_init (void)
{
  pygobject_type_spec_by_type = g_hash_table_new_full (NULL, NULL, NULL, NULL);
}

static int
PyGObject_init (PyGObject * self)
{
  self->handle = NULL;
  self->type = PYFRIDA_TYPE (GObject);

  self->signal_closures = NULL;

  return 0;
}

static void
PyGObject_dealloc (PyGObject * self)
{
  gpointer handle;

  handle = PyGObject_steal_handle (self);
  if (handle != NULL)
  {
    Py_BEGIN_ALLOW_THREADS
    self->type->destroy (handle);
    Py_END_ALLOW_THREADS
  }

  ((freefunc) PyType_GetSlot (Py_TYPE (self), Py_tp_free)) (self);
}

static void
PyGObject_gc_dealloc (PyGObject * self)
{
  PyObject_GC_UnTrack (self);

  PyGObject_dealloc (self);
}

static void
PyGObject_take_handle (PyGObject * self,
                       gpointer handle,
                       const PyGObjectType * type)
{
  self->handle = handle;
  self->type = type;

  if (handle != NULL)
    g_object_set_data (G_OBJECT (handle), "pyobject", self);
}

static gpointer
PyGObject_steal_handle (PyGObject * self)
{
  gpointer handle = self->handle;
  GSList * entry;

  if (handle == NULL)
    return NULL;

  for (entry = self->signal_closures; entry != NULL; entry = entry->next)
  {
    GClosure * closure = entry->data;

    g_signal_handlers_disconnect_matched (handle, G_SIGNAL_MATCH_CLOSURE, 0, 0, closure, NULL, NULL);
  }
  g_clear_pointer (&self->signal_closures, g_slist_free);

  g_object_set_data (G_OBJECT (handle), "pyobject", NULL);

  self->handle = NULL;

  return handle;
}

static void
PyGObject_register_type (GType instance_type,
                         PyGObjectType * python_type)
{
  g_hash_table_insert (pygobject_type_spec_by_type, GSIZE_TO_POINTER (instance_type), python_type);
}

static PyObject *
PyGObject_marshal_object (gpointer handle,
                          GType type)
{
  const PyGObjectType * pytype;

  if (handle == NULL)
    PyFrida_RETURN_NONE;

  pytype = g_hash_table_lookup (pygobject_type_spec_by_type, GSIZE_TO_POINTER (type));
  if (pytype == NULL)
    pytype = PYFRIDA_TYPE (GObject);

  return PyGObject_new_take_handle (g_object_ref (handle), pytype);
}

static PyObject *
PyGObject_new_take_handle (gpointer handle,
                           const PyGObjectType * pytype)
{
  PyObject * object;

  if (handle == NULL)
    PyFrida_RETURN_NONE;

  object = PyGObject_try_get_from_handle (handle);
  if (object == NULL)
  {
    object = PyObject_CallFunction (pytype->object, NULL);
    PyGObject_take_handle (PY_GOBJECT (object), handle, pytype);

    if (pytype->init_from_handle != NULL)
      pytype->init_from_handle (object, handle);
  }
  else
  {
    pytype->destroy (handle);
    Py_IncRef (object);
  }

  return object;
}

static PyObject *
PyGObject_try_get_from_handle (gpointer handle)
{
  return g_object_get_data (handle, "pyobject");
}

static PyObject *
PyGObject_on (PyGObject * self,
              PyObject * args)
{
  guint signal_id;
  PyObject * callback;
  guint max_arg_count, allowed_arg_count_including_sender;
  GSignalQuery query;
  GClosure * closure;

  if (!PyGObject_parse_signal_method_args (args, G_OBJECT_TYPE (self->handle), &signal_id, &callback))
    return NULL;

  max_arg_count = PyFrida_get_max_argument_count (callback);
  if (max_arg_count != G_MAXUINT)
  {
    g_signal_query (signal_id, &query);

    allowed_arg_count_including_sender = 1 + query.n_params;

    if (max_arg_count > allowed_arg_count_including_sender)
      goto too_many_arguments;
  }

  closure = PyGObject_make_closure_for_signal (callback, max_arg_count);
  g_signal_connect_closure_by_id (self->handle, signal_id, 0, closure, TRUE);

  self->signal_closures = g_slist_prepend (self->signal_closures, closure);

  PyFrida_RETURN_NONE;

too_many_arguments:
  {
    return PyErr_Format (PyExc_TypeError,
        "callback expects too many arguments, the '%s' signal only has %u but callback expects %u",
        g_signal_name (signal_id), query.n_params, max_arg_count);
  }
}

static PyObject *
PyGObject_off (PyGObject * self,
               PyObject * args)
{
  guint signal_id;
  PyObject * callback;
  GSList * entry;
  GClosure * closure;

  if (!PyGObject_parse_signal_method_args (args, G_OBJECT_TYPE (self->handle), &signal_id, &callback))
    return NULL;

  entry = g_slist_find_custom (self->signal_closures, callback, (GCompareFunc) PyGObject_compare_signal_closure_callback);
  if (entry == NULL)
    goto unknown_callback;

  closure = entry->data;
  self->signal_closures = g_slist_delete_link (self->signal_closures, entry);

  g_signal_handlers_disconnect_matched (self->handle, G_SIGNAL_MATCH_CLOSURE, signal_id, 0, closure, NULL, NULL);

  PyFrida_RETURN_NONE;

unknown_callback:
  {
    PyErr_SetString (PyExc_ValueError, "unknown callback");
    return NULL;
  }
}

static gboolean
PyGObject_parse_signal_method_args (PyObject * args,
                                    GType instance_type,
                                    guint * signal_id,
                                    PyObject ** callback)
{
  const gchar * signal_name;

  if (!PyArg_ParseTuple (args, "sO", &signal_name, callback))
    return FALSE;

  if (!PyCallable_Check (*callback))
  {
    PyErr_SetString (PyExc_TypeError, "second argument must be callable");
    return FALSE;
  }

  *signal_id = g_signal_lookup (signal_name, instance_type);
  if (*signal_id == 0)
    goto invalid_signal_name;

  return TRUE;

invalid_signal_name:
  {
    GString * message;
    guint * ids, n_ids, i;

    message = g_string_sized_new (128);

    g_string_append (message, PyGObject_class_name_from_c (g_type_name (instance_type)));

    ids = g_signal_list_ids (instance_type, &n_ids);

    if (n_ids > 0)
    {
      g_string_append_printf (message, " does not have a signal named '%s', it only has: ", signal_name);

      for (i = 0; i != n_ids; i++)
      {
        if (i != 0)
          g_string_append (message, ", ");
        g_string_append_c (message, '\'');
        g_string_append (message, g_signal_name (ids[i]));
        g_string_append_c (message, '\'');
      }
    }
    else
    {
      g_string_append (message, " does not have any signals");
    }

    g_free (ids);

    PyErr_SetString (PyExc_ValueError, message->str);

    g_string_free (message, TRUE);

    return FALSE;
  }
}

static gint
PyGObject_compare_signal_closure_callback (GClosure * closure,
                                           PyObject * callback)
{
  PyObject * registered = closure->data;
  PyObject * original;
  gboolean matches;

  if (PyObject_RichCompareBool (registered, callback, Py_EQ) == 1)
    return 0;

  original = PyObject_GetAttrString (registered, "_frida_original");
  if (original == NULL)
  {
    PyErr_Clear ();
    return -1;
  }

  matches = PyObject_RichCompareBool (original, callback, Py_EQ) == 1;
  Py_DecRef (original);

  return matches ? 0 : -1;
}

static GClosure *
PyGObject_make_closure_for_signal (PyObject * callback,
                                   guint max_arg_count)
{
  GClosure * closure;

  closure = g_closure_new_simple (sizeof (PyGObjectSignalClosure), callback);
  Py_IncRef (callback);

  g_closure_add_finalize_notifier (closure, callback, (GClosureNotify) PyGObjectSignalClosure_finalize);
  g_closure_set_marshal (closure, PyGObjectSignalClosure_marshal);

  PY_GOBJECT_SIGNAL_CLOSURE (closure)->max_arg_count = max_arg_count;

  return closure;
}

static void
PyGObjectSignalClosure_finalize (PyObject * callback)
{
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure ();
  Py_DecRef (callback);
  PyGILState_Release (gstate);
}

static void
PyGObjectSignalClosure_marshal (GClosure * closure,
                                GValue * return_gvalue,
                                guint n_param_values,
                                const GValue * param_values,
                                gpointer invocation_hint,
                                gpointer marshal_data)
{
  PyGObjectSignalClosure * self = PY_GOBJECT_SIGNAL_CLOSURE (closure);
  PyObject * callback = closure->data;
  PyGILState_STATE gstate;
  PyObject * args, * result;

  gstate = PyGILState_Ensure ();

  if (self->max_arg_count == n_param_values)
    args = PyGObjectSignalClosure_marshal_params (param_values, n_param_values);
  else
    args = PyGObjectSignalClosure_marshal_params (param_values + 1, MIN (n_param_values - 1, self->max_arg_count));
  if (args == NULL)
  {
    PyErr_Print ();
    goto beach;
  }

  result = PyObject_CallObject (callback, args);
  if (result != NULL)
    Py_DecRef (result);
  else
    PyErr_Print ();

  Py_DecRef (args);

beach:
  PyGILState_Release (gstate);
}

static PyObject *
PyGObjectSignalClosure_marshal_params (const GValue * params,
                                       guint params_length)
{
  PyObject * args;
  guint i;

  args = PyTuple_New (params_length);

  for (i = 0; i != params_length; i++)
  {
    PyObject * arg;

    arg = PyGObject_marshal_value (&params[i]);
    if (arg == NULL)
      goto marshal_error;

    PyTuple_SetItem (args, i, arg);
  }

  return args;

marshal_error:
  {
    Py_DecRef (args);
    return NULL;
  }
}

static PyObject *
PyGObject_marshal_value (const GValue * value)
{
  GType type = G_VALUE_TYPE (value);

  switch (type)
  {
    case G_TYPE_BOOLEAN:
      return PyBool_FromLong (g_value_get_boolean (value));
    case G_TYPE_INT:
      return PyLong_FromLong (g_value_get_int (value));
    case G_TYPE_UINT:
      return PyLong_FromUnsignedLong (g_value_get_uint (value));
    case G_TYPE_FLOAT:
      return PyFloat_FromDouble (g_value_get_float (value));
    case G_TYPE_DOUBLE:
      return PyFloat_FromDouble (g_value_get_double (value));
    case G_TYPE_STRING:
      return PyGObject_marshal_string (g_value_get_string (value));
    case G_TYPE_VARIANT:
      return PyGObject_marshal_variant (g_value_get_variant (value));
    default:
      break;
  }

  if (G_TYPE_IS_ENUM (type))
    return PyGObject_marshal_enum (g_value_get_enum (value), type);

  if (type == G_TYPE_BYTES)
    return PyGObject_marshal_bytes (g_value_get_boxed (value));

  if (G_TYPE_IS_OBJECT (type))
    return PyGObject_marshal_object (g_value_get_object (value), type);

  return PyErr_Format (PyExc_NotImplementedError, "unsupported type: '%s'", g_type_name (type));
}

static const gchar *
PyGObject_class_name_from_c (const gchar * cname)
{
  if (g_str_has_prefix (cname, "Frida"))
    return cname + 5;

  return cname;
}

static guint
PyFrida_get_max_argument_count (PyObject * callable)
{
  guint result = G_MAXUINT;
  PyObject * spec;
  PyObject * varargs;
  PyObject * args;
  PyObject * is_method;

  spec = PyObject_CallFunction (inspect_getargspec, "O", callable);
  if (spec == NULL)
  {
    PyErr_Clear ();
    goto beach;
  }

  varargs = PyTuple_GetItem (spec, 1);
  if (varargs != Py_None)
    goto beach;

  args = PyTuple_GetItem (spec, 0);
  result = PyObject_Size (args);

  is_method = PyObject_CallFunction (inspect_ismethod, "O", callable);
  if (is_method == Py_True)
    result--;
  Py_XDECREF (is_method);

beach:
  Py_XDECREF (spec);

  return result;
}

static gboolean
PyGObject_unmarshal_enum (const gchar * str,
                          GType type,
                          gpointer value)
{
  GEnumClass * enum_class;
  GEnumValue * enum_value;

  enum_class = g_type_class_ref (type);

  enum_value = g_enum_get_value_by_nick (enum_class, str);
  if (enum_value == NULL)
    goto invalid_value;

  *((gint *) value) = enum_value->value;

  g_type_class_unref (enum_class);

  return TRUE;

invalid_value:
  {
    GString * message;
    guint i;

    message = g_string_sized_new (128);

    g_string_append_printf (message,
        "enum type %s does not have a value named '%s', it only has: ",
        g_type_name (type), str);

    for (i = 0; i != enum_class->n_values; i++)
    {
      if (i != 0)
        g_string_append (message, ", ");
      g_string_append_c (message, '\'');
      g_string_append (message, enum_class->values[i].value_nick);
      g_string_append_c (message, '\'');
    }

    PyErr_SetString (PyExc_ValueError, message->str);

    g_string_free (message, TRUE);

    g_type_class_unref (enum_class);

    return FALSE;
  }
}

static PyObject *
PyGObject_marshal_enum (gint value,
                        GType type)
{
  GEnumClass * enum_class;
  GEnumValue * enum_value;
  PyObject * result;

  enum_class = g_type_class_ref (type);

  enum_value = g_enum_get_value (enum_class, value);
  result = PyUnicode_FromString (enum_value->value_nick);

  g_type_class_unref (enum_class);

  return result;
}

static gboolean
PyGObject_unmarshal_string (PyObject * value,
                           gchar ** str)
{
  PyObject * bytes;

  *str = NULL;

  bytes = PyUnicode_AsUTF8String (value);
  if (bytes == NULL)
    return FALSE;

  *str = g_strdup (PyBytes_AsString (bytes));

  Py_DecRef (bytes);

  return *str != NULL;
}

static PyObject *
PyGObject_marshal_string (const gchar * str)
{
  if (str == NULL)
    PyFrida_RETURN_NONE;

  return PyUnicode_FromString (str);
}

static gboolean
PyGObject_unmarshal_strv (PyObject * value,
                          gchar *** strv,
                          gint * length)
{
  gint n, i;
  gchar ** elements;

  if (value == Py_None)
  {
    *strv = NULL;
    *length = 0;
    return TRUE;
  }

  if (!PyList_Check (value) && !PyTuple_Check (value))
    goto invalid_type;

  n = PySequence_Size (value);
  elements = g_new0 (gchar *, n + 1);

  for (i = 0; i != n; i++)
  {
    PyObject * element, * bytes;

    element = PySequence_GetItem (value, i);
    bytes = PyUnicode_Check (element) ? PyUnicode_AsUTF8String (element) : NULL;
    Py_DecRef (element);
    if (bytes == NULL)
      goto invalid_element;

    elements[i] = g_strdup (PyBytes_AsString (bytes));
    Py_DecRef (bytes);
  }

  *strv = elements;
  *length = n;

  return TRUE;

invalid_type:
  {
    PyErr_SetString (PyExc_TypeError, "expected a list or tuple of strings");
    return FALSE;
  }
invalid_element:
  {
    g_strfreev (elements);

    PyErr_SetString (PyExc_TypeError, "expected a list or tuple of strings only");
    return FALSE;
  }
}

static PyObject *
PyGObject_marshal_strv (gchar * const * strv,
                        gint length)
{
  PyObject * result;
  gint i;

  if (strv == NULL)
    PyFrida_RETURN_NONE;

  result = PyList_New (length);

  for (i = 0; i != length; i++)
    PyList_SetItem (result, i, PyGObject_marshal_string (strv[i]));

  return result;
}

static PyObject *
PyGObject_marshal_bytes (GBytes * bytes)
{
  gconstpointer data;
  gsize size;

  if (bytes == NULL)
    PyFrida_RETURN_NONE;

  data = g_bytes_get_data (bytes, &size);

  return PyBytes_FromStringAndSize (data, size);
}

static gboolean
PyGObject_unmarshal_vardict (PyObject * value,
                            GHashTable ** dict)
{
  GHashTable * result;
  Py_ssize_t pos;
  PyObject * key, * val;

  if (!PyDict_Check (value))
    goto invalid_type;

  result = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) g_variant_unref);

  pos = 0;
  while (PyDict_Next (value, &pos, &key, &val))
  {
    PyObject * key_bytes;
    GVariant * variant;

    key_bytes = PyUnicode_Check (key) ? PyUnicode_AsUTF8String (key) : NULL;
    if (key_bytes == NULL)
      goto invalid_key;

    if (!PyGObject_unmarshal_variant (val, &variant))
    {
      Py_DecRef (key_bytes);
      goto propagate_error;
    }

    g_hash_table_insert (result, g_strdup (PyBytes_AsString (key_bytes)), variant);
    Py_DecRef (key_bytes);
  }

  *dict = result;

  return TRUE;

invalid_type:
  {
    PyErr_SetString (PyExc_TypeError, "expected a dictionary");
    return FALSE;
  }
invalid_key:
  {
    g_hash_table_unref (result);
    PyErr_SetString (PyExc_TypeError, "dictionary keys must be strings");
    return FALSE;
  }
propagate_error:
  {
    g_hash_table_unref (result);
    return FALSE;
  }
}

static PyObject *
PyGObject_marshal_vardict (GHashTable * dict)
{
  PyObject * result;
  GHashTableIter iter;
  const gchar * key;
  GVariant * raw_value;

  result = PyDict_New ();

  g_hash_table_iter_init (&iter, dict);

  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &raw_value))
  {
    PyObject * value = PyGObject_marshal_variant (raw_value);

    PyDict_SetItemString (result, key, value);

    Py_DecRef (value);
  }

  return result;
}

static gboolean
PyGObject_unmarshal_variant (PyObject * value,
                            GVariant ** variant)
{
  if (PyUnicode_Check (value))
  {
    PyObject * bytes = PyUnicode_AsUTF8String (value);
    if (bytes == NULL)
      return FALSE;
    *variant = g_variant_ref_sink (g_variant_new_string (PyBytes_AsString (bytes)));
    Py_DecRef (bytes);
    return TRUE;
  }

  if (PyBool_Check (value))
  {
    *variant = g_variant_ref_sink (g_variant_new_boolean (value == Py_True));
    return TRUE;
  }

  if (PyLong_Check (value))
  {
    long long l = PyLong_AsLongLong (value);
    if (l == -1 && PyErr_Occurred () != NULL)
      return FALSE;
    *variant = g_variant_ref_sink (g_variant_new_int64 (l));
    return TRUE;
  }

  if (PyFloat_Check (value))
  {
    *variant = g_variant_ref_sink (g_variant_new_double (PyFloat_AsDouble (value)));
    return TRUE;
  }

  if (PyBytes_Check (value))
  {
    char * data;
    Py_ssize_t size;

    PyBytes_AsStringAndSize (value, &data, &size);
    *variant = g_variant_ref_sink (
        g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, size, sizeof (guint8)));
    return TRUE;
  }

  if (PySequence_Check (value))
    return PyGObject_unmarshal_variant_from_sequence (value, variant);

  if (PyMapping_Check (value))
    return PyGObject_unmarshal_variant_from_mapping (value, variant);

  PyErr_SetString (PyExc_TypeError, "unsupported value type");
  return FALSE;
}

static gboolean
PyGObject_unmarshal_variant_from_sequence (PyObject * sequence,
                                          GVariant ** variant)
{
  gboolean is_tuple;
  GVariantBuilder builder;
  Py_ssize_t n, i;
  PyObject * item = NULL;

  is_tuple = PyTuple_Check (sequence);

  if (is_tuple && PyTuple_Size (sequence) == 2)
  {
    PyObject * type = PyTuple_GetItem (sequence, 0);

    if (PyUnicode_Check (type))
    {
      PyObject * type_bytes = PyUnicode_AsUTF8String (type);
      gboolean is_uint64_cast;

      if (type_bytes == NULL)
        return FALSE;
      is_uint64_cast = strcmp (PyBytes_AsString (type_bytes), "uint64") == 0;
      Py_DecRef (type_bytes);

      if (is_uint64_cast)
      {
        unsigned long long l = PyLong_AsUnsignedLongLong (PyTuple_GetItem (sequence, 1));
        if (l == (unsigned long long) -1 && PyErr_Occurred () != NULL)
          return FALSE;
        *variant = g_variant_ref_sink (g_variant_new_uint64 (l));
        return TRUE;
      }
    }
  }

  g_variant_builder_init (&builder, is_tuple ? G_VARIANT_TYPE_TUPLE : G_VARIANT_TYPE ("av"));

  n = PySequence_Length (sequence);
  if (n == -1)
    goto propagate_error;

  for (i = 0; i != n; i++)
  {
    GVariant * child;

    item = PySequence_GetItem (sequence, i);
    if (item == NULL)
      goto propagate_error;

    if (!PyGObject_unmarshal_variant (item, &child))
      goto propagate_error;

    if (is_tuple)
      g_variant_builder_add_value (&builder, child);
    else
      g_variant_builder_add (&builder, "v", child);

    g_variant_unref (child);
    Py_DecRef (item);
    item = NULL;
  }

  *variant = g_variant_ref_sink (g_variant_builder_end (&builder));

  return TRUE;

propagate_error:
  {
    Py_XDECREF (item);
    g_variant_builder_clear (&builder);
    return FALSE;
  }
}

static gboolean
PyGObject_unmarshal_variant_from_mapping (PyObject * mapping,
                                         GVariant ** variant)
{
  GVariantBuilder builder;
  PyObject * items;
  Py_ssize_t n, i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  items = PyMapping_Items (mapping);
  if (items == NULL)
    goto propagate_error;

  n = PyList_Size (items);

  for (i = 0; i != n; i++)
  {
    PyObject * pair, * key, * val, * key_bytes;
    GVariant * child;

    pair = PyList_GetItem (items, i);
    key = PyTuple_GetItem (pair, 0);
    val = PyTuple_GetItem (pair, 1);

    key_bytes = PyUnicode_Check (key) ? PyUnicode_AsUTF8String (key) : NULL;
    if (key_bytes == NULL)
      goto propagate_error;

    if (!PyGObject_unmarshal_variant (val, &child))
    {
      Py_DecRef (key_bytes);
      goto propagate_error;
    }

    g_variant_builder_add (&builder, "{sv}", PyBytes_AsString (key_bytes), child);

    g_variant_unref (child);
    Py_DecRef (key_bytes);
  }

  Py_DecRef (items);

  *variant = g_variant_ref_sink (g_variant_builder_end (&builder));

  return TRUE;

propagate_error:
  {
    Py_XDECREF (items);
    g_variant_builder_clear (&builder);
    return FALSE;
  }
}

static PyObject *
PyGObject_marshal_variant (GVariant * variant)
{
  switch (g_variant_classify (variant))
  {
    case G_VARIANT_CLASS_STRING:
      return PyGObject_marshal_string (g_variant_get_string (variant, NULL));
    case G_VARIANT_CLASS_BYTE:
      return PyLong_FromLong (g_variant_get_byte (variant));
    case G_VARIANT_CLASS_INT16:
      return PyLong_FromLong (g_variant_get_int16 (variant));
    case G_VARIANT_CLASS_UINT16:
      return PyLong_FromUnsignedLong (g_variant_get_uint16 (variant));
    case G_VARIANT_CLASS_INT32:
      return PyLong_FromLong (g_variant_get_int32 (variant));
    case G_VARIANT_CLASS_UINT32:
      return PyLong_FromUnsignedLong (g_variant_get_uint32 (variant));
    case G_VARIANT_CLASS_INT64:
      return PyLong_FromLongLong (g_variant_get_int64 (variant));
    case G_VARIANT_CLASS_UINT64:
      return PyLong_FromUnsignedLongLong (g_variant_get_uint64 (variant));
    case G_VARIANT_CLASS_DOUBLE:
      return PyFloat_FromDouble (g_variant_get_double (variant));
    case G_VARIANT_CLASS_BOOLEAN:
      return PyBool_FromLong (g_variant_get_boolean (variant));
    case G_VARIANT_CLASS_ARRAY:
      if (g_variant_is_of_type (variant, G_VARIANT_TYPE ("ay")))
        return PyGObject_marshal_variant_byte_array (variant);

      if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT))
        return PyGObject_marshal_variant_dict (variant);

      return PyGObject_marshal_variant_array (variant);
    case G_VARIANT_CLASS_TUPLE:
      return PyGObject_marshal_variant_tuple (variant);
    case G_VARIANT_CLASS_VARIANT:
    {
      GVariant * inner;
      PyObject * result;

      inner = g_variant_get_variant (variant);
      result = PyGObject_marshal_variant (inner);
      g_variant_unref (inner);

      return result;
    }
    default:
      break;
  }

  PyFrida_RETURN_NONE;
}

static PyObject *
PyGObject_marshal_variant_byte_array (GVariant * variant)
{
  gconstpointer elements;
  gsize n_elements;

  elements = g_variant_get_fixed_array (variant, &n_elements, sizeof (guint8));

  return PyBytes_FromStringAndSize (elements, n_elements);
}

static PyObject *
PyGObject_marshal_variant_dict (GVariant * variant)
{
  PyObject * dict;
  GVariantIter iter;
  gchar * key;
  GVariant * raw_value;

  dict = PyDict_New ();

  g_variant_iter_init (&iter, variant);

  while (g_variant_iter_next (&iter, "{sv}", &key, &raw_value))
  {
    PyObject * value = PyGObject_marshal_variant (raw_value);

    PyDict_SetItemString (dict, key, value);

    Py_DecRef (value);
    g_variant_unref (raw_value);
    g_free (key);
  }

  return dict;
}

static PyObject *
PyGObject_marshal_variant_array (GVariant * variant)
{
  GVariantIter iter;
  PyObject * list;
  guint i;
  GVariant * child;

  g_variant_iter_init (&iter, variant);

  list = PyList_New (g_variant_iter_n_children (&iter));

  for (i = 0; (child = g_variant_iter_next_value (&iter)) != NULL; i++)
  {
    PyList_SetItem (list, i, PyGObject_marshal_variant (child));

    g_variant_unref (child);
  }

  return list;
}

static PyObject *
PyGObject_marshal_variant_tuple (GVariant * variant)
{
  GVariantIter iter;
  PyObject * tuple;
  guint i;
  GVariant * child;

  g_variant_iter_init (&iter, variant);

  tuple = PyTuple_New (g_variant_iter_n_children (&iter));

  for (i = 0; (child = g_variant_iter_next_value (&iter)) != NULL; i++)
  {
    PyTuple_SetItem (tuple, i, PyGObject_marshal_variant (child));

    g_variant_unref (child);
  }

  return tuple;
}

static gboolean
PyGObject_unmarshal_certificate (PyObject * value,
                                 GTlsCertificate ** certificate)
{
  PyObject * bytes;
  const gchar * str;
  GError * error = NULL;

  if (value == Py_None)
  {
    *certificate = NULL;
    return TRUE;
  }

  bytes = PyUnicode_AsUTF8String (value);
  if (bytes == NULL)
    return FALSE;
  str = PyBytes_AsString (bytes);

  if (strchr (str, '\n') != NULL)
    *certificate = g_tls_certificate_new_from_pem (str, -1, &error);
  else
    *certificate = g_tls_certificate_new_from_file (str, &error);

  Py_DecRef (bytes);

  if (error != NULL)
    goto propagate_error;

  return TRUE;

propagate_error:
  {
    PyFrida_raise (g_error_new_literal (FRIDA_ERROR, FRIDA_ERROR_INVALID_ARGUMENT, error->message));
    g_error_free (error);

    return FALSE;
  }
}

static PyObject *
PyGObject_marshal_certificate (GTlsCertificate * certificate)
{
  PyObject * result;
  gchar * pem;

  if (certificate == NULL)
    PyFrida_RETURN_NONE;

  g_object_get (certificate, "certificate-pem", &pem, NULL);

  result = PyGObject_marshal_string (pem);

  g_free (pem);

  return result;
}

static PyObject *
PyFrida_raise (GError * error)
{
  PyObject * exception = PyFrida_marshal_error (error);

  PyErr_SetObject ((PyObject *) Py_TYPE (exception), exception);

  Py_DecRef (exception);

  return NULL;
}

static PyObject *
PyFrida_marshal_error (GError * error)
{
  PyObject * exception_class;
  GString * message;
  PyObject * instance;

  if (error->domain == FRIDA_ERROR)
    exception_class = g_hash_table_lookup (frida_exception_by_error_code, GINT_TO_POINTER (error->code));
  else
    exception_class = cancelled_exception;
  if (exception_class == NULL)
    exception_class = cancelled_exception;

  message = g_string_new ("");
  g_string_append_unichar (message, g_unichar_tolower (g_utf8_get_char (error->message)));
  g_string_append (message, g_utf8_offset_to_pointer (error->message, 1));

  instance = PyObject_CallFunction (exception_class, "s", message->str);

  g_string_free (message, TRUE);
  g_error_free (error);

  return instance;
}

static void
PyFrida_object_decref (gpointer obj)
{
  PyObject * o = obj;
  Py_DecRef (o);
}

static void
PyFrida_deliver (PyObject * callback,
                 PyObject * result,
                 PyObject * error)
{
  PyObject * outcome = PyObject_CallFunctionObjArgs (callback, result, error, NULL);
  if (outcome != NULL)
    Py_DecRef (outcome);
  else
    PyErr_Print ();
}

static PyObject *
PyFrida_make_completion (GTask * task,
                         PyFridaCompleteFunc complete)
{
  PyFridaRequest * request = g_slice_new (PyFridaRequest);
  request->task = task;
  request->complete = complete;

  return PyCapsule_New (request, "frida.request", NULL);
}

static PyObject *
PyFrida_complete_request (PyObject * module,
                          PyObject * args)
{
  PyObject * capsule, * value, * error;
  PyFridaRequest * request;

  if (!PyArg_ParseTuple (args, "OOO", &capsule, &value, &error))
    return NULL;

  request = PyCapsule_GetPointer (capsule, "frida.request");

  request->complete (request->task, value, error);
  g_object_unref (request->task);

  g_slice_free (PyFridaRequest, request);

  PyFrida_RETURN_NONE;
}

static gboolean
PyFrida_return_error (GTask * task,
                      PyObject * error)
{
  gchar * message;

  if (error == Py_None)
    return FALSE;

  message = NULL;
  {
    PyObject * text = PyObject_Str (error);
    if (text != NULL)
    {
      PyGObject_unmarshal_string (text, &message);
      Py_DecRef (text);
    }
  }

  g_task_return_new_error (task, FRIDA_ERROR, FRIDA_ERROR_INVALID_ARGUMENT,
      "%s", (message != NULL) ? message : "internal error");

  g_free (message);

  return TRUE;
}
