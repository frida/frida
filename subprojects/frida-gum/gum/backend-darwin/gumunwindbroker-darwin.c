/*
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumunwindbroker-priv.h"

#include "guminterceptor.h"
#include "gumprocess.h"

#include <capstone.h>
#include <gum/gumdarwin.h>
#include <gum/gummemory.h>
#include <ptrauth.h>

#define GUM_MH_MAGIC_64 0xfeedfacf
#define GUM_LIBDYLD_PATH "/usr/lib/system/libdyld.dylib"
#define GUM_LIBUNWIND_PATH "/usr/lib/system/libunwind.dylib"
#define GUM_UNWIND_CURSOR_VTABLE_OFFSET_SET_INFO 0x68
#define GUM_UNWIND_CURSOR_VTABLE_OFFSET_GET_REG 0x18
#define GUM_FP_TO_SP(fp) ((fp) + 0x10)
#ifdef HAVE_ARM64
# define GUM_UNWIND_CURSOR_unwindInfoMissing 0x268
# define GUM_UNWAARCH64_X29 29
# define GUM_STRIP_MASK 0x0000007fffffffffULL
#else
# define GUM_UNWIND_CURSOR_unwindInfoMissing 0x100
# define GUM_UNWX86_64_RBP 6
#endif

#if __has_feature (ptrauth_calls)
# define GUM_RESIGN_PTR(x) \
    GSIZE_TO_POINTER ( \
        gum_sign_code_address (gum_strip_code_address (GUM_ADDRESS (x))))
#else
# define GUM_RESIGN_PTR(x) (x)
#endif

typedef struct _GumDyldUnwindSections GumDyldUnwindSections;
typedef struct _GumLibunwindHook GumLibunwindHook;
typedef int (* GumDyldFindUnwindSectionsFunc) (void * addr, void * info);

struct _GumDyldUnwindSections
{
  const void * mh;
  const void * dwarf_section;
  uintptr_t dwarf_section_length;
  const void * compact_unwind_section;
  uintptr_t compact_unwind_section_length;
};

struct _GumLibunwindHook
{
  gpointer vtable;
  gssize shift;
  gpointer * set_info_slot;
  gpointer set_info_original;
  void (* set_info) (gpointer cursor, gint is_return_address);
  gpointer (* get_reg) (gpointer cursor, gint reg);
  GumInterceptor * interceptor;
};

static GumDyldFindUnwindSectionsFunc gum_unwind_dyld_find_sections_original;
static GumInterceptor * gum_unwind_libdyld_interceptor = NULL;
static GumLibunwindHook * gum_unwind_libunwind_hook = NULL;

static int gum_unwind_broker_replacement_dyld_find_unwind_sections (
    void * addr, void * info);
static void gum_unwind_broker_install_libunwind_hook (void);
static void gum_unwind_broker_uninstall_libunwind_hook (void);
static void gum_unwind_broker_replacement_set_info (gpointer cursor,
    gint is_return_address);
static GumAddress gum_unwind_broker_translate_pc (GumAddress code_address);
static gpointer gum_unwind_broker_find_libunwind_vtable (void);
static gboolean gum_unwind_broker_compute_vtable_shift (gpointer vtable,
    gssize * shift);
#ifdef HAVE_ARM64
static gboolean gum_unwind_broker_find_bss_range (
    const GumSectionDetails * details, GumMemoryRange * range);
#else
static gboolean gum_unwind_broker_is_empty_function (GumAddress address);
static gboolean gum_unwind_broker_has_first_match (GumAddress address,
    gsize size, gboolean * matches);
#endif

void
_gum_unwind_broker_backend_activate (void)
{
  GumModule * libdyld;
  GumAddress export;

  libdyld = gum_process_find_module_by_name (GUM_LIBDYLD_PATH);
  g_assert (libdyld != NULL);

  export = gum_module_find_export_by_name (libdyld,
      "_dyld_find_unwind_sections");
  g_assert (export != 0);

  g_object_unref (libdyld);

  gum_unwind_dyld_find_sections_original =
      (GumDyldFindUnwindSectionsFunc) GSIZE_TO_POINTER (export);

  gum_unwind_libdyld_interceptor = gum_interceptor_obtain ();
  gum_interceptor_replace (gum_unwind_libdyld_interceptor,
      gum_unwind_dyld_find_sections_original,
      gum_unwind_broker_replacement_dyld_find_unwind_sections, NULL, NULL);

  gum_unwind_broker_install_libunwind_hook ();
}

void
_gum_unwind_broker_backend_deactivate (void)
{
  gum_unwind_broker_uninstall_libunwind_hook ();

  if (gum_unwind_libdyld_interceptor != NULL)
  {
    gum_interceptor_revert (gum_unwind_libdyld_interceptor,
        gum_unwind_dyld_find_sections_original);
    g_object_unref (gum_unwind_libdyld_interceptor);
    gum_unwind_libdyld_interceptor = NULL;
    gum_unwind_dyld_find_sections_original = NULL;
  }
}

static int
gum_unwind_broker_replacement_dyld_find_unwind_sections (void * addr,
                                                         void * info)
{
  GumAddress address;

  address = GUM_ADDRESS (addr);
#ifdef HAVE_ARM64
  address &= 0x7ffffffffULL;
#endif

  if (_gum_unwind_broker_dispatch_sections (address, info))
    return 1;

  return gum_unwind_dyld_find_sections_original (addr, info);
}

static void
gum_unwind_broker_install_libunwind_hook (void)
{
#if GLIB_SIZEOF_VOID_P == 8
  GumLibunwindHook * hook;
  gpointer * set_info_slot;
  gpointer get_reg_impl;

  if (gum_unwind_libunwind_hook != NULL)
    return;

  hook = g_slice_new0 (GumLibunwindHook);

  hook->vtable = gum_unwind_broker_find_libunwind_vtable ();
  if (hook->vtable == NULL)
    goto unsupported_version;

  if (!gum_unwind_broker_compute_vtable_shift (hook->vtable, &hook->shift))
    goto unsupported_version;

  set_info_slot = (gpointer *) (GUM_ADDRESS (hook->vtable) +
      GUM_UNWIND_CURSOR_VTABLE_OFFSET_SET_INFO + hook->shift);
  get_reg_impl = *(gpointer *) (GUM_ADDRESS (hook->vtable) +
      GUM_UNWIND_CURSOR_VTABLE_OFFSET_GET_REG + hook->shift);

  hook->set_info_slot = set_info_slot;
  hook->set_info_original = *set_info_slot;
  hook->set_info = GUM_RESIGN_PTR (hook->set_info_original);
  hook->get_reg = GUM_RESIGN_PTR (get_reg_impl);

  hook->interceptor = gum_interceptor_obtain ();

  if (gum_interceptor_replace (hook->interceptor, hook->set_info_original,
        gum_unwind_broker_replacement_set_info, NULL, NULL) != GUM_REPLACE_OK)
    goto unsupported_version;

  gum_unwind_libunwind_hook = hook;
  return;

unsupported_version:
  g_clear_object (&hook->interceptor);
  g_slice_free (GumLibunwindHook, hook);
#endif
}

static void
gum_unwind_broker_uninstall_libunwind_hook (void)
{
  GumLibunwindHook * hook = gum_unwind_libunwind_hook;

  if (hook == NULL)
    return;
  gum_unwind_libunwind_hook = NULL;

  gum_interceptor_revert (hook->interceptor, hook->set_info_original);
  g_object_unref (hook->interceptor);
  g_slice_free (GumLibunwindHook, hook);
}

static void
gum_unwind_broker_replacement_set_info (gpointer self,
                                        gint is_return_address)
{
  GumLibunwindHook * hook = gum_unwind_libunwind_hook;
  GumAddress fp, stored_pc, translated;
  gpointer * stored_pc_slot;
  gboolean missing_info;
#if defined (HAVE_ARM64) && !__has_feature (ptrauth_calls)
  gboolean was_signed = FALSE;
#endif

  if (hook == NULL)
    return;

  hook->set_info (self, is_return_address);

#ifdef HAVE_ARM64
  fp = GUM_ADDRESS (hook->get_reg (self, GUM_UNWAARCH64_X29));
#else
  fp = GUM_ADDRESS (hook->get_reg (self, GUM_UNWX86_64_RBP));
#endif
  if (fp == 0 || fp == -1)
    return;

  missing_info = *((guint8 *) self + GUM_UNWIND_CURSOR_unwindInfoMissing);
  if (missing_info)
    return;

  stored_pc_slot = GSIZE_TO_POINTER (fp + GLIB_SIZEOF_VOID_P);
  stored_pc = GUM_ADDRESS (*stored_pc_slot);
#if __has_feature (ptrauth_calls)
  stored_pc = gum_strip_code_address (stored_pc);
#elif defined (HAVE_ARM64)
  was_signed = (stored_pc & ~GUM_STRIP_MASK) != 0ULL;
  if (was_signed)
    stored_pc &= GUM_STRIP_MASK;
#endif

  translated = gum_unwind_broker_translate_pc (stored_pc);
  if (translated == 0)
    return;

#if __has_feature (ptrauth_calls)
  *stored_pc_slot = ptrauth_sign_unauthenticated (
      ptrauth_strip (GSIZE_TO_POINTER (translated), ptrauth_key_asia),
      ptrauth_key_asib, GUM_FP_TO_SP (fp));
#elif defined (HAVE_ARM64)
  if (was_signed)
  {
    GumAddress resigned;

    asm volatile (
        "mov x17, %1\n\t"
        "mov x16, %2\n\t"
        ".byte 0x5f, 0x21, 0x03, 0xd5\n\t" /* pacib1716 */
        "mov %0, x17\n\t"
        : "=r" (resigned)
        : "r" (translated & GUM_STRIP_MASK),
          "r" (GUM_FP_TO_SP (fp))
        : "x16", "x17"
    );

    *stored_pc_slot = GSIZE_TO_POINTER (resigned);
  }
  else
  {
    *stored_pc_slot = GSIZE_TO_POINTER (translated);
  }
#else
  *stored_pc_slot = GSIZE_TO_POINTER (translated);
#endif
}

static GumAddress
gum_unwind_broker_translate_pc (GumAddress code_address)
{
  gpointer translated;
  GumAddress result;

  translated = gum_invocation_stack_translate (
      gum_interceptor_get_current_stack (), GSIZE_TO_POINTER (code_address));
  if (translated != GSIZE_TO_POINTER (code_address))
    return GUM_ADDRESS (translated);

  result = _gum_unwind_broker_dispatch_translate (code_address);
  if (result == code_address)
    return 0;

  return result;
}

static gpointer
gum_unwind_broker_find_libunwind_vtable (void)
{
  GumAddress result = 0;
  GumModule * libunwind;
  GumAddress export;
  uint64_t address;
  G_GNUC_UNUSED cs_err err;
  csh capstone;
  cs_insn * insn = NULL;
  const uint8_t * code;
  size_t size;
  const size_t max_size = 2048;

  libunwind = gum_process_find_module_by_name (GUM_LIBUNWIND_PATH);
  if (libunwind == NULL)
    goto beach;

  export = gum_module_find_export_by_name (libunwind, "unw_init_local");
  if (export == 0)
    export = gum_module_find_export_by_name (libunwind,
        "_Unwind_RaiseException");
  if (export == 0)
    goto beach;
  export = gum_strip_code_address (export);
  address = export;

#ifdef HAVE_ARM64
  cs_arch_register_arm64 ();
  err = cs_open (CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &capstone);
#else
  cs_arch_register_x86 ();
  err = cs_open (CS_ARCH_X86, CS_MODE_64, &capstone);
#endif
  g_assert (err == CS_ERR_OK);

  err = cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  g_assert (err == CS_ERR_OK);

  insn = cs_malloc (capstone);
  code = GSIZE_TO_POINTER (export);
  size = max_size;

#ifdef HAVE_ARM64
  {
    GumAddress last_adrp;
    guint last_adrp_reg;
    GumMemoryRange bss_range;

    bss_range.base_address = 0;
    gum_module_enumerate_sections (libunwind,
        (GumFoundSectionFunc) gum_unwind_broker_find_bss_range, &bss_range);

    while (cs_disasm_iter (capstone, &code, &size, &address, insn))
    {
      if (insn->id == ARM64_INS_RET || insn->id == ARM64_INS_RETAA ||
          insn->id == ARM64_INS_RETAB)
        break;
      if (insn->id == ARM64_INS_ADRP)
      {
        if (result != 0)
          break;
        last_adrp = (GumAddress) insn->detail->arm64.operands[1].imm;
        last_adrp_reg = insn->detail->arm64.operands[0].reg;
      }
      else if (insn->id == ARM64_INS_ADD &&
          insn->detail->arm64.operands[0].reg == last_adrp_reg)
      {
        GumAddress candidate;
        gboolean is_bss;

        candidate = last_adrp +
            (GumAddress) insn->detail->arm64.operands[2].imm;

        is_bss = bss_range.base_address != 0 &&
            bss_range.base_address <= candidate &&
            candidate < bss_range.base_address + bss_range.size;
        if (!is_bss)
        {
          if (result == 0)
          {
            result = candidate;
            last_adrp = candidate;
          }
          else
          {
            result = candidate;
            break;
          }
        }
      }
      else if (result != 0)
      {
        break;
      }
    }
  }
#else
  while (cs_disasm_iter (capstone, &code, &size, &address, insn))
  {
    if (insn->id == X86_INS_RET)
      break;
    if (insn->id == X86_INS_LEA)
    {
      const cs_x86_op * op = &insn->detail->x86.operands[1];
      if (op->type == X86_OP_MEM && op->mem.base == X86_REG_RIP)
      {
        result = address + op->mem.disp * op->mem.scale;
        break;
      }
    }
  }
#endif

  if (insn != NULL)
    cs_free (insn, 1);
  cs_close (&capstone);

beach:
  g_clear_object (&libunwind);

  return GSIZE_TO_POINTER (result);
}

#ifdef HAVE_ARM64

static gboolean
gum_unwind_broker_find_bss_range (const GumSectionDetails * details,
                                  GumMemoryRange * range)
{
  if (strcmp (details->name, "__bss") == 0)
  {
    range->base_address = details->address;
    range->size = details->size;
    return FALSE;
  }

  return TRUE;
}

static gboolean
gum_unwind_broker_compute_vtable_shift (gpointer vtable,
                                        gssize * shift)
{
  gboolean result = FALSE;
  G_GNUC_UNUSED cs_err err;
  csh capstone;
  cs_insn * insn = NULL;
  const uint8_t * code;
  uint64_t address;
  size_t size = 4;

  cs_arch_register_arm64 ();
  err = cs_open (CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &capstone);
  g_assert (err == CS_ERR_OK);

  insn = cs_malloc (capstone);
  code = gum_strip_code_pointer (*(gpointer *) vtable);
  address = GPOINTER_TO_SIZE (code);

  if (cs_disasm_iter (capstone, &code, &size, &address, insn))
  {
    if (insn->id == ARM64_INS_RET || insn->id == ARM64_INS_RETAA ||
        insn->id == ARM64_INS_RETAB)
      *shift = 0;
    else
      *shift = -2 * GLIB_SIZEOF_VOID_P;

    result = TRUE;
  }

  if (insn != NULL)
    cs_free (insn, 1);
  cs_close (&capstone);

  return result;
}

#else

static gboolean
gum_unwind_broker_compute_vtable_shift (gpointer vtable,
                                        gssize * shift)
{
  GumAddress cursor = GPOINTER_TO_SIZE (vtable);
  GumAddress error = cursor + 16 * GLIB_SIZEOF_VOID_P;

  while (cursor < error && *(gpointer *) GSIZE_TO_POINTER (cursor) == NULL)
    cursor += GLIB_SIZEOF_VOID_P;
  if (cursor == error)
    return FALSE;

  if (gum_unwind_broker_is_empty_function (
        GUM_ADDRESS (*(gpointer *) GSIZE_TO_POINTER (cursor))) &&
      gum_unwind_broker_is_empty_function (
        GUM_ADDRESS (*(gpointer *) GSIZE_TO_POINTER (
            cursor + GLIB_SIZEOF_VOID_P))))
  {
    *shift = cursor - GPOINTER_TO_SIZE (vtable);
  }
  else
  {
    *shift = cursor - GPOINTER_TO_SIZE (vtable) - 2 * GLIB_SIZEOF_VOID_P;
  }

  return TRUE;
}

static gboolean
gum_unwind_broker_is_empty_function (GumAddress address)
{
  gboolean matches = FALSE;
  GumMemoryRange range;
  GumMatchPattern * pattern;

  range.base_address = address;
  range.size = 6;

  pattern = gum_match_pattern_new_from_string ("55 48 89 e5 5d c3");

  gum_memory_scan (&range, pattern,
      (GumMemoryScanMatchFunc) gum_unwind_broker_has_first_match, &matches);

  gum_match_pattern_unref (pattern);

  return matches;
}

static gboolean
gum_unwind_broker_has_first_match (GumAddress address,
                                   gsize size,
                                   gboolean * matches)
{
  *matches = TRUE;
  return FALSE;
}

#endif
