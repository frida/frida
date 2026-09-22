#include <stdbool.h>
#include <stdlib.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

typedef struct _FridaMachO FridaMachO;

struct _FridaMachO
{
  const void * base;
  uintptr_t slide;
  uint64_t size;
  const void * linkedit;
  const struct symtab_command * symtab;
  const struct dysymtab_command * dysymtab;
};

static void frida_parse_macho (const void * macho, FridaMachO * result);

static void frida_append_string (char ** output, const char * val);
static void frida_append_char (char ** output, char val);
static void frida_append_uint64 (char ** output, uint64_t val);

static bool frida_str_equals (const char * str, const char * other);
static bool frida_str_has_prefix (const char * str, const char * prefix);
static bool frida_str_contains (const char * str, const char * needle);
static const char * frida_strstr (const char * str, const char * needle);

size_t
frida_fetch_dyld_symbols (char * output_buffer, const void * dyld_load_address)
{
  FridaMachO dyld;
  size_t size;
  const struct nlist_64 * symbols;
  const char * strings;
  char * cursor;
  uint32_t n, i;

  frida_parse_macho (dyld_load_address, &dyld);

  symbols = dyld.linkedit + dyld.symtab->symoff;
  strings = dyld.linkedit + dyld.symtab->stroff;

  cursor = output_buffer;
  n = 0;

  for (i = dyld.dysymtab->ilocalsym; i != dyld.dysymtab->nlocalsym; i++)
  {
    const struct nlist_64 * sym = &symbols[i];
    const char * name = strings + sym->n_un.n_strx;

    if (frida_str_contains (name, "libdyld_initialize") ||
        frida_str_contains (name, "notifyObjCInit") ||
        frida_str_contains (name, "restartWithDyldInCache") ||
        frida_str_equals (name, "_gProcessInfo") ||
        frida_str_equals (name, "__ZN5dyld412gProcessInfoE") ||
        frida_str_equals (name, "__ZL12sProcessInfo") ||
        frida_str_contains (name, "launchWithClosure") ||
        frida_str_contains (name, "initializeMainExecutable") ||
        frida_str_contains (name, "registerThreadHelpers") ||
        frida_str_has_prefix (name, "_dlopen") ||
        frida_str_has_prefix (name, "_strcmp") ||
        frida_str_contains (name, "doModInitFunctions") ||
        frida_str_contains (name, "doGetDOFSections"))
    {
      if (n != 0)
        frida_append_char (&cursor, '\n');

      frida_append_uint64 (&cursor, (uint64_t) (dyld.base + sym->n_value));
      frida_append_char (&cursor, '\t');
      frida_append_string (&cursor, name);

      n++;
    }
  }

  frida_append_char (&cursor, '\n');
  frida_append_uint64 (&cursor, dyld.size);
  frida_append_char (&cursor, '\t');
  frida_append_string (&cursor, "dyld_size");

  size = cursor - output_buffer;

  frida_append_char (&cursor, '\0');

  return size;
}

static void
frida_parse_macho (const void * macho, FridaMachO * result)
{
  const struct mach_header_64 * header;
  const struct load_command * lc;
  uint32_t i;
  const void * preferred_base;
  const void * linkedit;

  header = macho;
  lc = (const struct load_command *) (header + 1);

  preferred_base = NULL;
  linkedit = NULL;

  for (i = 0; i != header->ncmds; i++)
  {
    switch (lc->cmd)
    {
      case LC_SEGMENT_64:
      {
        const struct segment_command_64 * sc = (const struct segment_command_64 *) lc;

        if (frida_str_equals (sc->segname, "__TEXT"))
        {
          preferred_base = (const void *) sc->vmaddr;
          result->size = sc->vmsize;
        }
        else if (frida_str_equals (sc->segname, "__LINKEDIT"))
        {
          linkedit = (const void *) sc->vmaddr - sc->fileoff;
        }

        break;
      }
      case LC_SYMTAB:
        result->symtab = (const struct symtab_command *) lc;
        break;
      case LC_DYSYMTAB:
        result->dysymtab = (const struct dysymtab_command *) lc;
        break;
      default:
        break;
    }

    lc = (const struct load_command *) ((uint8_t *) lc + lc->cmdsize);
  }

  result->base = macho;
  result->slide = macho - preferred_base;
  result->linkedit = linkedit + result->slide;
}

static void
frida_append_string (char ** output, const char * val)
{
  char * cursor = *output;
  char c;

  while ((c = *val++) != '\0')
    *cursor++ = c;

  *output = cursor;
}

static void
frida_append_char (char ** output, char val)
{
  char * cursor = *output;

  *cursor++ = val;

  *output = cursor;
}

static void
frida_append_uint64 (char ** output, uint64_t val)
{
  const char nibble_to_hex_char[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  char * cursor = *output;
  bool found_first_nonzero;
  int shift;

  found_first_nonzero = false;
  for (shift = 64 - 4; shift != -4; shift -= 4)
  {
    uint8_t nibble = (val >> shift) & 0xf;

    if (!found_first_nonzero && nibble != 0)
      found_first_nonzero = true;

    if (found_first_nonzero)
      *cursor++ = nibble_to_hex_char[nibble];
  }

  if (!found_first_nonzero)
    *cursor++ = '0';

  *output = cursor;
}

static bool
frida_str_equals (const char * str, const char * other)
{
  char a, b;

  do
  {
    a = *str;
    b = *other;
    if (a != b)
      return false;
    str++;
    other++;
  }
  while (a != '\0');

  return true;
}

static bool
frida_str_has_prefix (const char * str, const char * prefix)
{
  char c;

  while ((c = *prefix++) != '\0')
  {
    if (*str++ != c)
      return false;
  }

  return true;
}

static bool
frida_str_contains (const char * str, const char * needle)
{
  return frida_strstr (str, needle) != NULL;
}

static const char *
frida_strstr (const char * str, const char * needle)
{
  char first, c;

  first = needle[0];

  while ((c = *str) != '\0')
  {
    if (c == first && frida_str_has_prefix (str, needle))
      return str;
    str++;
  }

  return NULL;
}

#ifdef BUILDING_TEST_PROGRAM

#include <assert.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach-o/dyld_images.h>

int
main (void)
{
  mach_port_t task;
  struct task_dyld_info info;
  mach_msg_type_number_t count;
  kern_return_t kr;
  const struct dyld_all_image_infos * dyld_info;
  char output_buffer[128 * 1024];
  size_t size;

  task = mach_task_self ();

  count = TASK_DYLD_INFO_COUNT;
  kr = task_info (task, TASK_DYLD_INFO, (task_info_t) &info, &count);
  assert (kr == KERN_SUCCESS);

  dyld_info = (const struct dyld_all_image_infos *) info.all_image_info_addr;

  size = frida_fetch_dyld_symbols (output_buffer, dyld_info->dyldImageLoadAddress);

  printf ("%s\n", output_buffer);
  printf ("size: %zu bytes\n", size);

  return 0;
}

#endif
