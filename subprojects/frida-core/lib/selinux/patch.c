#include "frida-selinux.h"

#include <fcntl.h>
#include <gio/gio.h>
#include <selinux/selinux.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>

#define FRIDA_SELINUX_ERROR frida_selinux_error_quark ()

typedef struct _FridaSELinuxRule FridaSELinuxRule;
typedef enum _FridaSELinuxErrorEnum FridaSELinuxErrorEnum;

struct _FridaSELinuxRule
{
  const gchar * sources[4];
  const gchar * target;
  const gchar * klass;
  const gchar * permissions[16];
};

enum _FridaSELinuxErrorEnum
{
  FRIDA_SELINUX_ERROR_POLICY_FORMAT_NOT_SUPPORTED,
  FRIDA_SELINUX_ERROR_TYPE_NOT_FOUND,
  FRIDA_SELINUX_ERROR_CLASS_NOT_FOUND,
  FRIDA_SELINUX_ERROR_PERMISSION_NOT_FOUND
};

static gboolean frida_load_policy (const gchar * filename, policydb_t * db, gchar ** data, GError ** error);
static gboolean frida_save_policy (const gchar * filename, policydb_t * db, GError ** error);
static type_datum_t * frida_ensure_type (policydb_t * db, const gchar * type_name, guint num_attributes, ...);
static void frida_add_type_to_class_constraints_referencing_attribute (policydb_t * db, uint32_t type_id, uint32_t attribute_id);
static gboolean frida_ensure_permissive (policydb_t * db, const gchar * type_name, GError ** error);
static avtab_datum_t * frida_ensure_rule (policydb_t * db, const gchar * s, const gchar * t, const gchar * c, const gchar * p, GError ** error);

static gboolean frida_set_file_contents (const gchar * filename, const gchar * contents, gssize length, GError ** error);

static const FridaSELinuxRule frida_selinux_rules[] =
{
  { { "domain", NULL }, "domain", "process", { "execmem", NULL } },
  { { "domain", NULL }, "frida_file", "dir", { "search", NULL } },
  { { "domain", NULL }, "frida_file", "file", { "open", "read", "getattr", "execute", "?map", NULL } },
  { { "domain", NULL }, "frida_memfd", "file", { "open", "read", "write", "getattr", "execute", "?map", NULL } },
  { { "domain", NULL }, "shell_data_file", "dir", { "search", NULL } },
  { { "domain", NULL }, "zygote_exec", "file", { "execute", NULL } },
  { { "domain", NULL }, "$self", "process", { "sigchld", NULL } },
  { { "domain", NULL }, "$self", "fd", { "use", NULL } },
  { { "domain", NULL }, "$self", "unix_stream_socket", { "connectto", "read", "write", "getattr", "getopt", NULL } },
  { { "domain", NULL }, "$self", "tcp_socket", { "read", "write", "getattr", "getopt", NULL } },
  { { "zygote", NULL }, "zygote", "capability", { "sys_ptrace", NULL } },
  { { "?app_zygote", NULL }, "zygote_exec", "file", { "read", NULL } },
  { { "system_server", NULL, }, "?apex_art_data_file", "file", { "execute", NULL } },
};

G_DEFINE_QUARK (frida-selinux-error-quark, frida_selinux_error)

void
frida_selinux_patch_policy (void)
{
  const gchar * system_policy = "/sys/fs/selinux/policy";
  policydb_t db;
  gchar * db_data;
  sidtab_t sidtab;
  GError * error = NULL;
  int res G_GNUC_UNUSED;
  guint rule_index;

  sepol_set_policydb (&db);
  sepol_set_sidtab (&sidtab);

  if (!g_file_test (system_policy, G_FILE_TEST_EXISTS))
    return;

  if (!frida_load_policy (system_policy, &db, &db_data, &error))
  {
    g_printerr ("Unable to load SELinux policy from the kernel: %s\n", error->message);
    g_error_free (error);
    return;
  }

  res = policydb_load_isids (&db, &sidtab);
  g_assert (res == 0);

  if (frida_ensure_type (&db, "frida_file", 2, "file_type", "mlstrustedobject", &error) == NULL)
  {
    g_printerr ("Unable to add SELinux type: %s\n", error->message);
    g_clear_error (&error);
    goto beach;
  }

  if (frida_ensure_type (&db, "frida_memfd", 2, "file_type", "mlstrustedobject", &error) == NULL)
  {
    g_printerr ("Unable to add SELinux type: %s\n", error->message);
    g_clear_error (&error);
    goto beach;
  }

  for (rule_index = 0; rule_index != G_N_ELEMENTS (frida_selinux_rules); rule_index++)
  {
    const FridaSELinuxRule * rule = &frida_selinux_rules[rule_index];
    const gchar * target = rule->target;
    const gchar * const * source_cursor;
    const gchar * const * perm_entry;

    if (target[0] == '?')
    {
      target++;

      if (hashtab_search (db.p_types.table, (char *) target) == NULL)
        continue;
    }

    for (source_cursor = rule->sources; *source_cursor != NULL; source_cursor++)
    {
      const gchar * source = *source_cursor;

      if (source[0] == '?')
      {
        source++;

        if (hashtab_search (db.p_types.table, (char *) source) == NULL)
          continue;
      }

      for (perm_entry = rule->permissions; *perm_entry != NULL; perm_entry++)
      {
        const gchar * perm = *perm_entry;
        gboolean is_important = TRUE;

        if (perm[0] == '?')
        {
          is_important = FALSE;
          perm++;
        }

        if (frida_ensure_rule (&db, source, target, rule->klass, perm, &error) == NULL)
        {
          if (!g_error_matches (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_PERMISSION_NOT_FOUND) || is_important)
            g_printerr ("Unable to add SELinux rule: %s\n", error->message);
          g_clear_error (&error);
        }
      }
    }
  }

  if (!frida_save_policy ("/sys/fs/selinux/load", &db, &error))
  {
    gboolean success = FALSE, probably_in_emulator;

    probably_in_emulator = security_getenforce () == 1 && security_setenforce (0) == 0;
    if (probably_in_emulator)
    {
      g_clear_error (&error);

      success = frida_ensure_permissive (&db, "shell", &error);
      if (success)
        success = frida_save_policy ("/sys/fs/selinux/load", &db, &error);

      security_setenforce (1);
    }

    if (!success)
    {
      g_printerr ("Unable to save SELinux policy to the kernel: %s\n", error->message);
      g_clear_error (&error);
    }
  }

beach:
  policydb_destroy (&db);
  g_free (db_data);
}

static gboolean
frida_load_policy (const gchar * filename, policydb_t * db, gchar ** data, GError ** error)
{
  policy_file_t file;
  int res;

  policy_file_init (&file);
  file.type = PF_USE_MEMORY;
  if (!g_file_get_contents (filename, &file.data, &file.len, error))
    return FALSE;

  *data = file.data;

  policydb_init (db);

  res = policydb_read (db, &file, FALSE);
  if (res != 0)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_POLICY_FORMAT_NOT_SUPPORTED, "unsupported policy database format");
    policydb_destroy (db);
    g_free (*data);
    return FALSE;
  }

  return TRUE;
}

static gboolean
frida_save_policy (const gchar * filename, policydb_t * db, GError ** error)
{
  void * data;
  size_t size;
  int res G_GNUC_UNUSED;

  res = policydb_to_image (NULL, db, &data, &size);
  g_assert (res == 0);

  return frida_set_file_contents (filename, data, size, error);
}

static type_datum_t *
frida_ensure_type (policydb_t * db, const gchar * type_name, guint n_attributes, ...)
{
  type_datum_t * type;
  uint32_t type_id;
  va_list vl;
  guint i;
  GError * pending_error, ** error;

  type = hashtab_search (db->p_types.table, (char *) type_name);
  if (type == NULL)
  {
    uint32_t i, n;
    gchar * name;

    type_id = ++db->p_types.nprim;
    name = strdup (type_name);

    type = malloc (sizeof (type_datum_t));

    type_datum_init (type);
    type->s.value = type_id;
    type->primary = TRUE;
    type->flavor = TYPE_TYPE;

    hashtab_insert (db->p_types.table, name, type);

    policydb_index_others (NULL, db, FALSE);

    i = type_id - 1;
    n = db->p_types.nprim;
    db->type_attr_map = realloc (db->type_attr_map, n * sizeof (ebitmap_t));
    db->attr_type_map = realloc (db->attr_type_map, n * sizeof (ebitmap_t));
    ebitmap_init (&db->type_attr_map[i]);
    ebitmap_init (&db->attr_type_map[i]);

    /* We also need to add the type itself as the degenerate case. */
    ebitmap_set_bit (&db->type_attr_map[i], i, 1);
  }
  else
  {
    type_id = type->s.value;
  }

  va_start (vl, n_attributes);

  pending_error = NULL;
  for (i = 0; i != n_attributes; i++)
  {
    const gchar * attribute_name;
    type_datum_t * attribute_type;

    attribute_name = va_arg (vl, const gchar *);
    attribute_type = hashtab_search (db->p_types.table, (char *) attribute_name);
    if (attribute_type != NULL)
    {
      uint32_t attribute_id = attribute_type->s.value;
      ebitmap_set_bit (&attribute_type->types, type_id - 1, 1);
      ebitmap_set_bit (&db->type_attr_map[type_id - 1], attribute_id - 1, 1);
      ebitmap_set_bit (&db->attr_type_map[attribute_id - 1], type_id - 1, 1);

      frida_add_type_to_class_constraints_referencing_attribute (db, type_id, attribute_id);
    }
    else if (pending_error == NULL)
    {
      g_set_error (&pending_error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_TYPE_NOT_FOUND, "attribute type “%s” does not exist", attribute_name);
    }
  }

  error = va_arg (vl, GError **);
  if (pending_error != NULL)
    g_propagate_error (error, pending_error);

  va_end (vl);

  return (pending_error == NULL) ? type : NULL;
}

static void
frida_add_type_to_class_constraints_referencing_attribute (policydb_t * db, uint32_t type_id, uint32_t attribute_id)
{
  uint32_t class_index;

  for (class_index = 0; class_index != db->p_classes.nprim; class_index++)
  {
    class_datum_t * klass = db->class_val_to_struct[class_index];
    constraint_node_t * node;

    for (node = klass->constraints; node != NULL; node = node->next)
    {
      constraint_expr_t * expr;

      for (expr = node->expr; expr != NULL; expr = expr->next)
      {
        ebitmap_node_t * tnode;
        guint i;

        ebitmap_for_each_bit (&expr->type_names->types, tnode, i)
        {
          if (ebitmap_node_get_bit (tnode, i) && i == attribute_id - 1)
            ebitmap_set_bit (&expr->names, type_id - 1, 1);
        }
      }
    }
  }
}

static gboolean
frida_ensure_permissive (policydb_t * db, const gchar * type_name, GError ** error)
{
  type_datum_t * type;
  int res G_GNUC_UNUSED;

  type = hashtab_search (db->p_types.table, (char *) type_name);
  if (type == NULL)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_TYPE_NOT_FOUND, "type %s does not exist", type_name);
    return FALSE;
  }

  res = ebitmap_set_bit (&db->permissive_map, type->s.value, 1);
  g_assert (res == 0);

  return TRUE;
}

static avtab_datum_t *
frida_ensure_rule (policydb_t * db, const gchar * s, const gchar * t, const gchar * c, const gchar * p, GError ** error)
{
  type_datum_t * source, * target;
  gchar * self_type = NULL;
  class_datum_t * klass;
  perm_datum_t * perm;
  avtab_key_t key;
  avtab_datum_t * av;
  uint32_t perm_bit;

  source = hashtab_search (db->p_types.table, (char *) s);
  if (source == NULL)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_TYPE_NOT_FOUND, "source type “%s” does not exist", s);
    return NULL;
  }

  if (strcmp (t, "$self") == 0)
  {
    char * self_context;
    gchar ** tokens;

    getcon (&self_context);

    tokens = g_strsplit (self_context, ":", 4);

    self_type = g_strdup (tokens[2]);
    t = self_type;

    g_strfreev (tokens);

    freecon (self_context);
  }

  target = hashtab_search (db->p_types.table, (char *) t);

  g_free (self_type);

  if (target == NULL)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_TYPE_NOT_FOUND, "target type “%s” does not exist", t);
    return NULL;
  }

  klass = hashtab_search (db->p_classes.table, (char *) c);
  if (klass == NULL)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_CLASS_NOT_FOUND, "class “%s” does not exist", c);
    return NULL;
  }

  perm = hashtab_search (klass->permissions.table, (char *) p);
  if (perm == NULL && klass->comdatum != NULL)
    perm = hashtab_search (klass->comdatum->permissions.table, (char *) p);
  if (perm == NULL)
  {
    g_set_error (error, FRIDA_SELINUX_ERROR, FRIDA_SELINUX_ERROR_PERMISSION_NOT_FOUND, "perm “%s” does not exist on the “%s” class", p, c);
    return NULL;
  }
  perm_bit = 1U << (perm->s.value - 1);

  key.source_type = source->s.value;
  key.target_type = target->s.value;
  key.target_class = klass->s.value;
  key.specified = AVTAB_ALLOWED;

  av = avtab_search (&db->te_avtab, &key);
  if (av == NULL)
  {
    int res G_GNUC_UNUSED;

    av = malloc (sizeof (avtab_datum_t));
    av->data = perm_bit;
    av->xperms = NULL;

    res = avtab_insert (&db->te_avtab, &key, av);
    g_assert (res == 0);
  }

  av->data |= perm_bit;

  return av;
}

/* Just like g_file_set_contents() except there's no temporary file involved. */

static gboolean
frida_set_file_contents (const gchar * filename, const gchar * contents, gssize length, GError ** error)
{
  int fd, res;
  gsize offset, size;

  fd = open (filename, O_RDWR);
  if (fd == -1)
    goto error;

  offset = 0;
  size = (length == -1) ? strlen (contents) : length;

  while (offset != size)
  {
    res = write (fd, contents + offset, size - offset);
    if (res != -1)
      offset += res;
    else if (errno != EINTR)
      goto error;
  }

  close (fd);

  return TRUE;

error:
  {
    int e;

    e = errno;
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (e), "%s", g_strerror (e));

    if (fd != -1)
      close (fd);

    return FALSE;
  }
}

