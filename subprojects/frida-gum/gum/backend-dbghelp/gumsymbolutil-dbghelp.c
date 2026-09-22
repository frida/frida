/*
 * Copyright (C) 2008-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Matt Oh <oh.jeongwook@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsymbolutil.h"

#include "gum/gumdbghelp.h"

#include <psapi.h>

/* HACK: don't have access to this enum */
#define GUM_SymTagFunction       5
#define GUM_SymTagPublicSymbol  10

typedef struct _GumSymbolInfo GumSymbolInfo;

#pragma pack (push)
#pragma pack (1)

struct _GumSymbolInfo
{
  SYMBOL_INFO sym_info;
  gchar sym_name_buf[GUM_MAX_SYMBOL_NAME + 1];
};

#pragma pack (pop)

static BOOL CALLBACK enum_functions_callback (SYMBOL_INFO * sym_info,
    gulong symbol_size, gpointer user_context);
static gboolean is_function (SYMBOL_INFO * sym_info);

gboolean
gum_symbol_details_from_address (gpointer address,
                                 GumDebugSymbolDetails * details)
{
  GumDbghelpImpl * dbghelp;
  GumSymbolInfo si = { 0, };
  IMAGEHLP_LINE64 li = { 0, };
  DWORD displacement_dw;
  DWORD64 displacement_qw;
  BOOL has_sym_info, has_file_info;

  dbghelp = gum_dbghelp_impl_try_obtain ();
  if (dbghelp == NULL)
    return FALSE;

  memset (details, 0, sizeof (GumDebugSymbolDetails));
  details->address = GUM_ADDRESS (address);

  si.sym_info.SizeOfStruct = sizeof (SYMBOL_INFO);
  si.sym_info.MaxNameLen = sizeof (si.sym_name_buf);

  li.SizeOfStruct = sizeof (li);

  dbghelp->Lock ();

  has_sym_info = dbghelp->SymFromAddr (GetCurrentProcess (),
      GPOINTER_TO_SIZE (address), &displacement_qw, &si.sym_info);
  if (has_sym_info)
  {
    HMODULE mod = GSIZE_TO_POINTER (si.sym_info.ModBase);

    GetModuleBaseNameA (GetCurrentProcess (), mod, details->module_name,
        sizeof (details->module_name) - 1);
    g_strlcpy (details->symbol_name, si.sym_info.Name,
        sizeof (details->symbol_name));
  }

  has_file_info = dbghelp->SymGetLineFromAddr64 (GetCurrentProcess (),
      GPOINTER_TO_SIZE (address), &displacement_dw, &li);
  if (has_file_info)
  {
    g_strlcpy (details->file_name, li.FileName, sizeof (details->file_name));
    details->line_number = li.LineNumber;
    details->column = displacement_dw;
  }

  dbghelp->Unlock ();

  return (has_sym_info || has_file_info);
}

gchar *
gum_symbol_name_from_address (gpointer address)
{
  GumDebugSymbolDetails details;

  if (gum_symbol_details_from_address (address, &details))
    return g_strdup (details.symbol_name);
  else
    return NULL;
}

gpointer
gum_find_function (const gchar * name)
{
  gpointer result = NULL;
  GArray * matches;

  matches = gum_find_functions_matching (name);
  if (matches->len >= 1)
    result = g_array_index (matches, gpointer, 0);
  g_array_free (matches, TRUE);

  return result;
}

GArray *
gum_find_functions_named (const gchar * name)
{
  return gum_find_functions_matching (name);
}

GArray *
gum_find_functions_matching (const gchar * str)
{
  GArray * matches;
  GumDbghelpImpl * dbghelp;
  gchar * match_formatted_str;
  HANDLE cur_process_handle;
  guint64 any_module_base;

  matches = g_array_new (FALSE, FALSE, sizeof (gpointer));

  dbghelp = gum_dbghelp_impl_try_obtain ();
  if (dbghelp == NULL)
    return matches;

  if (strchr (str, '!') == NULL)
    match_formatted_str = g_strconcat ("*!", str, NULL);
  else
    match_formatted_str = g_strdup (str);

  cur_process_handle = GetCurrentProcess ();
  any_module_base = 0;

  dbghelp->Lock ();
  dbghelp->SymEnumSymbols (cur_process_handle, any_module_base,
      match_formatted_str, enum_functions_callback, matches);
  dbghelp->Unlock ();

  g_free (match_formatted_str);

  return matches;
}

gboolean
gum_load_symbols (const gchar * path)
{
  gboolean success = FALSE;
  GumDbghelpImpl * dbghelp;
  WCHAR * path_utf16;
  DWORD64 base;

  dbghelp = gum_dbghelp_impl_try_obtain ();
  if (dbghelp == NULL)
    return FALSE;

  path_utf16 = (WCHAR *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

  base = GPOINTER_TO_SIZE (GetModuleHandleW (path_utf16));
  if (base == 0)
    goto beach;

  dbghelp->Lock ();
  base = dbghelp->SymLoadModuleExW (GetCurrentProcess (), NULL, path_utf16,
      NULL, base, 0, NULL, 0);
  success = base != 0 || GetLastError () == ERROR_SUCCESS;
  dbghelp->Unlock ();

beach:
  g_free (path_utf16);

  return success;
}

static BOOL CALLBACK
enum_functions_callback (SYMBOL_INFO * sym_info,
                         gulong symbol_size,
                         gpointer user_context)
{
  GArray * result = user_context;

  if (is_function (sym_info))
  {
    gpointer address = GSIZE_TO_POINTER (sym_info->Address);
    g_array_append_val (result, address);
  }

  return TRUE;
}

static gboolean
is_function (SYMBOL_INFO * sym_info)
{
  gboolean result;

  switch (sym_info->Tag)
  {
    case GUM_SymTagFunction:
    case GUM_SymTagPublicSymbol:
      result = TRUE;
      break;
    default:
      result = FALSE;
      break;
  }

  return result;
}
