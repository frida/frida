/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumx86writer.h"

#include "gumlibc.h"
#include "gummemory.h"

typedef guint GumX86MetaReg;
typedef struct _GumX86RegInfo GumX86RegInfo;
typedef guint GumX86LabelRefSize;
typedef struct _GumX86LabelRef GumX86LabelRef;

enum _GumX86MetaReg
{
  GUM_X86_META_XAX = 0,
  GUM_X86_META_XCX,
  GUM_X86_META_XDX,
  GUM_X86_META_XBX,
  GUM_X86_META_XSP,
  GUM_X86_META_XBP,
  GUM_X86_META_XSI,
  GUM_X86_META_XDI,
  GUM_X86_META_R8,
  GUM_X86_META_R9,
  GUM_X86_META_R10,
  GUM_X86_META_R11,
  GUM_X86_META_R12,
  GUM_X86_META_R13,
  GUM_X86_META_R14,
  GUM_X86_META_R15
};

struct _GumX86RegInfo
{
  GumX86MetaReg meta;
  guint width;
  guint index;
  gboolean index_is_extended;
};

enum _GumX86LabelRefSize
{
  GUM_LREF_SHORT,
  GUM_LREF_NEAR,
  GUM_LREF_ABS
};

struct _GumX86LabelRef
{
  gconstpointer id;
  guint8 * address;
  GumX86LabelRefSize size;
};

static void gum_x86_writer_put_argument_list_setup (GumX86Writer * self,
    GumCallingConvention conv, guint n_args, const GumArgument * args);
static void gum_x86_writer_put_argument_list_setup_va (GumX86Writer * self,
    GumCallingConvention conv, guint n_args, va_list args);
static void gum_x86_writer_put_argument_list_teardown (GumX86Writer * self,
    GumCallingConvention conv, guint n_args);
static void gum_x86_writer_put_aligned_argument_list_setup (GumX86Writer * self,
    GumCallingConvention conv, guint n_args, const GumArgument * args);
static void gum_x86_writer_put_aligned_argument_list_setup_va (
    GumX86Writer * self, GumCallingConvention conv, guint n_args, va_list args);
static void gum_x86_writer_put_aligned_argument_list_teardown (
    GumX86Writer * self, GumCallingConvention conv, guint n_args);
static guint gum_x86_writer_get_needed_alignment_correction (
    GumX86Writer * self, guint n_args);
static gboolean gum_x86_writer_put_short_jmp (GumX86Writer * self,
    gconstpointer target);
static gboolean gum_x86_writer_put_near_jmp (GumX86Writer * self,
    gconstpointer target);
static void gum_x86_writer_put_ud2 (GumX86Writer * self);
static gboolean gum_x86_writer_put_fx_save_or_restore_reg_ptr (
    GumX86Writer * self, guint8 operation, GumX86Reg reg);
static gboolean gum_x86_writer_put_vector_base_offset (GumX86Writer * self,
    guint map, guint pp, gboolean w, guint length, guint8 opcode, guint reg,
    guint vvvv, guint tuple, GumX86Reg base_reg, gssize offset,
    gboolean has_imm, guint8 imm);
static gboolean gum_x86_writer_put_kmov_base_offset (GumX86Writer * self,
    guint8 opcode, guint mask_reg, GumX86Reg base_reg, gssize offset);
static void gum_x86_writer_describe_cpu_reg (GumX86Writer * self,
    GumX86Reg reg, GumX86RegInfo * ri);

static GumX86MetaReg gum_meta_reg_from_cpu_reg (GumX86Reg reg);

static gboolean gum_x86_writer_put_prefix_for_reg_info (GumX86Writer * self,
    const GumX86RegInfo * ri, guint operand_index);
static gboolean gum_x86_writer_put_prefix_for_registers (GumX86Writer * self,
    const GumX86RegInfo * width_reg, guint default_width, ...);

static guint8 gum_get_jcc_opcode (x86_insn instruction_id);

GumX86Writer *
gum_x86_writer_new (gpointer code_address)
{
  GumX86Writer * writer;

  writer = g_slice_new (GumX86Writer);

  gum_x86_writer_init (writer, code_address);

  return writer;
}

GumX86Writer *
gum_x86_writer_ref (GumX86Writer * writer)
{
  g_atomic_int_inc (&writer->ref_count);

  return writer;
}

void
gum_x86_writer_unref (GumX86Writer * writer)
{
  if (g_atomic_int_dec_and_test (&writer->ref_count))
  {
    gum_x86_writer_clear (writer);

    g_slice_free (GumX86Writer, writer);
  }
}

void
gum_x86_writer_init (GumX86Writer * writer,
                     gpointer code_address)
{
  writer->ref_count = 1;
  writer->flush_on_destroy = TRUE;

  writer->label_defs = NULL;
  writer->label_refs.data = NULL;

  gum_x86_writer_reset (writer, code_address);
}

static gboolean
gum_x86_writer_has_label_defs (GumX86Writer * self)
{
  return self->label_defs != NULL;
}

static gboolean
gum_x86_writer_has_label_refs (GumX86Writer * self)
{
  return self->label_refs.data != NULL;
}

void
gum_x86_writer_clear (GumX86Writer * writer)
{
  if (writer->flush_on_destroy)
    gum_x86_writer_flush (writer);

  if (gum_x86_writer_has_label_defs (writer))
    gum_metal_hash_table_unref (writer->label_defs);

  if (gum_x86_writer_has_label_refs (writer))
    gum_metal_array_free (&writer->label_refs);
}

void
gum_x86_writer_reset (GumX86Writer * writer,
                      gpointer code_address)
{
#if GLIB_SIZEOF_VOID_P == 4
  writer->target_cpu = GUM_CPU_IA32;
#else
  writer->target_cpu = GUM_CPU_AMD64;
#endif
  writer->target_abi = GUM_NATIVE_ABI;
  writer->cpu_features = gum_query_cpu_features ();

  writer->base = (guint8 *) code_address;
  writer->code = (guint8 *) code_address;
  writer->pc = GUM_ADDRESS (code_address);

  if (gum_x86_writer_has_label_defs (writer))
    gum_metal_hash_table_remove_all (writer->label_defs);

  if (gum_x86_writer_has_label_refs (writer))
    gum_metal_array_remove_all (&writer->label_refs);
}

void
gum_x86_writer_set_target_cpu (GumX86Writer * self,
                               GumCpuType cpu_type)
{
  self->target_cpu = cpu_type;
}

void
gum_x86_writer_set_target_abi (GumX86Writer * self,
                               GumAbiType abi_type)
{
  self->target_abi = abi_type;
}

gpointer
gum_x86_writer_cur (GumX86Writer * self)
{
  return self->code;
}

guint
gum_x86_writer_offset (GumX86Writer * self)
{
  return self->code - self->base;
}

static void
gum_x86_writer_commit (GumX86Writer * self,
                       guint n)
{
  self->code += n;
  self->pc += n;
}

gboolean
gum_x86_writer_flush (GumX86Writer * self)
{
  guint num_refs, ref_index;

  if (!gum_x86_writer_has_label_refs (self))
    return TRUE;

  if (!gum_x86_writer_has_label_defs (self))
    return FALSE;

  num_refs = self->label_refs.length;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumX86LabelRef * r;
    gpointer target_address;
    gint32 distance;

    r = gum_metal_array_element_at (&self->label_refs, ref_index);

    target_address = gum_metal_hash_table_lookup (self->label_defs, r->id);
    if (target_address == NULL)
      goto error;

    distance = (gssize) target_address - (gssize) r->address;

    switch (r->size)
    {
      case GUM_LREF_SHORT:
        if (!GUM_IS_WITHIN_INT8_RANGE (distance))
          goto error;
        *((gint8 *) (r->address - 1)) = distance;
        break;
      case GUM_LREF_NEAR:
        *((gint32 *) (r->address - 4)) = GINT32_TO_LE (distance);
        break;
      case GUM_LREF_ABS:
      {
        goffset target_offset;
        GumAddress base_pc, target_pc;

        target_offset = (guint8 *) target_address - self->base;

        base_pc = self->pc - gum_x86_writer_offset (self);
        target_pc = base_pc + target_offset;

        if (self->target_cpu == GUM_CPU_AMD64)
          *((guint64 *) (r->address - 8)) = GUINT64_TO_LE (target_pc);
        else
          *((guint32 *) (r->address - 4)) = GUINT32_TO_LE (target_pc);

        break;
      }
      default:
        g_assert_not_reached ();
    }
  }

  gum_metal_array_remove_all (&self->label_refs);

  return TRUE;

error:
  {
    gum_metal_array_remove_all (&self->label_refs);

    return FALSE;
  }
}

GumX86Reg
gum_x86_writer_get_cpu_register_for_nth_argument (GumX86Writer * self,
                                                  guint n)
{
  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (self->target_abi == GUM_ABI_UNIX)
    {
      static const GumX86Reg amd64_unix_reg_by_index[] = {
        GUM_X86_RDI,
        GUM_X86_RSI,
        GUM_X86_RDX,
        GUM_X86_RCX,
        GUM_X86_R8,
        GUM_X86_R9
      };

      if (n < G_N_ELEMENTS (amd64_unix_reg_by_index))
        return amd64_unix_reg_by_index[n];
    }
    else if (self->target_abi == GUM_ABI_WINDOWS)
    {
      static const GumX86Reg amd64_windows_reg_by_index[] = {
        GUM_X86_RCX,
        GUM_X86_RDX,
        GUM_X86_R8,
        GUM_X86_R9
      };

      if (n < G_N_ELEMENTS (amd64_windows_reg_by_index))
        return amd64_windows_reg_by_index[n];
    }
  }
  else if (self->target_cpu == GUM_CPU_IA32)
  {
    static const GumX86Reg fastcall_reg_by_index[] = {
      GUM_X86_ECX,
      GUM_X86_EDX,
    };

    if (n < G_N_ELEMENTS (fastcall_reg_by_index))
      return fastcall_reg_by_index[n];
  }

  return GUM_X86_NONE;
}

gboolean
gum_x86_writer_put_label (GumX86Writer * self,
                          gconstpointer id)
{
  if (!gum_x86_writer_has_label_defs (self))
    self->label_defs = gum_metal_hash_table_new (NULL, NULL);

  if (gum_metal_hash_table_lookup (self->label_defs, id) != NULL)
    return FALSE;

  gum_metal_hash_table_insert (self->label_defs, (gpointer) id, self->code);

  return TRUE;
}

static void
gum_x86_writer_add_label_reference_here (GumX86Writer * self,
                                         gconstpointer id,
                                         GumX86LabelRefSize size)
{
  GumX86LabelRef * r;

  if (!gum_x86_writer_has_label_refs (self))
    gum_metal_array_init (&self->label_refs, sizeof (GumX86LabelRef));

  r = gum_metal_array_append (&self->label_refs);
  r->id = id;
  r->address = self->code;
  r->size = size;
}

gboolean
gum_x86_writer_can_branch_directly_between (GumAddress from,
                                            GumAddress to)
{
  gint64 distance;
  gboolean distance_fits_in_i32;

  distance = (gssize) to - (gssize) (from + 5);

  distance_fits_in_i32 = (distance >= G_MININT32 && distance <= G_MAXINT32);

  return distance_fits_in_i32;
}

gboolean
gum_x86_writer_put_call_address_with_arguments (GumX86Writer * self,
                                                GumCallingConvention conv,
                                                GumAddress func,
                                                guint n_args,
                                                ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_address (self, func))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_address_with_arguments_array (GumX86Writer * self,
                                                      GumCallingConvention conv,
                                                      GumAddress func,
                                                      guint n_args,
                                                      const GumArgument * args)
{
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_address (self, func))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_address_with_aligned_arguments (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumAddress func,
    guint n_args,
    ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_aligned_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_address (self, func))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_address_with_aligned_arguments_array (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumAddress func,
    guint n_args,
    const GumArgument * args)
{
  gum_x86_writer_put_aligned_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_address (self, func))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_with_arguments (GumX86Writer * self,
                                            GumCallingConvention conv,
                                            GumX86Reg reg,
                                            guint n_args,
                                            ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_reg (self, reg))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_with_arguments_array (GumX86Writer * self,
                                                  GumCallingConvention conv,
                                                  GumX86Reg reg,
                                                  guint n_args,
                                                  const GumArgument * args)
{
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_reg (self, reg))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_with_aligned_arguments (GumX86Writer * self,
                                                    GumCallingConvention conv,
                                                    GumX86Reg reg,
                                                    guint n_args,
                                                    ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_aligned_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_reg (self, reg))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_with_aligned_arguments_array (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumX86Reg reg,
    guint n_args,
    const GumArgument * args)
{
  gum_x86_writer_put_aligned_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_reg (self, reg))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

static void
gum_x86_writer_put_argument_list_setup (GumX86Writer * self,
                                        GumCallingConvention conv,
                                        guint n_args,
                                        const GumArgument * args)
{
  gint arg_index;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    for (arg_index = (gint) n_args - 1; arg_index >= 0; arg_index--)
    {
      const GumArgument * arg = &args[arg_index];

      if (arg->type == GUM_ARG_ADDRESS)
      {
        gum_x86_writer_put_push_u32 (self, arg->value.address);
      }
      else
      {
        gum_x86_writer_put_push_reg (self, arg->value.reg);
      }
    }
  }
  else
  {
    static const GumX86Reg reg_for_arg_unix_64[6] = {
      GUM_X86_RDI,
      GUM_X86_RSI,
      GUM_X86_RDX,
      GUM_X86_RCX,
      GUM_X86_R8,
      GUM_X86_R9
    };
    static const GumX86Reg reg_for_arg_unix_32[6] = {
      GUM_X86_EDI,
      GUM_X86_ESI,
      GUM_X86_EDX,
      GUM_X86_ECX,
      GUM_X86_R8D,
      GUM_X86_R9D
    };
    static const GumX86Reg reg_for_arg_windows_64[4] = {
      GUM_X86_RCX,
      GUM_X86_RDX,
      GUM_X86_R8,
      GUM_X86_R9
    };
    static const GumX86Reg reg_for_arg_windows_32[4] = {
      GUM_X86_ECX,
      GUM_X86_EDX,
      GUM_X86_R8D,
      GUM_X86_R9D
    };
    const GumX86Reg * reg_for_arg_64, * reg_for_arg_32;
    gint reg_for_arg_count;

    if (self->target_abi == GUM_ABI_UNIX)
    {
      reg_for_arg_64 = reg_for_arg_unix_64;
      reg_for_arg_32 = reg_for_arg_unix_32;
      reg_for_arg_count = G_N_ELEMENTS (reg_for_arg_unix_64);
    }
    else
    {
      reg_for_arg_64 = reg_for_arg_windows_64;
      reg_for_arg_32 = reg_for_arg_windows_32;
      reg_for_arg_count = G_N_ELEMENTS (reg_for_arg_windows_64);
    }

    for (arg_index = (gint) n_args - 1; arg_index >= 0; arg_index--)
    {
      const GumArgument * arg = &args[arg_index];

      if (arg_index < reg_for_arg_count)
      {
        if (arg->type == GUM_ARG_ADDRESS)
        {
          gum_x86_writer_put_mov_reg_u64 (self, reg_for_arg_64[arg_index],
              arg->value.address);
        }
        else if (gum_meta_reg_from_cpu_reg (arg->value.reg) !=
            gum_meta_reg_from_cpu_reg (reg_for_arg_64[arg_index]))
        {
          if (arg->value.reg >= GUM_X86_EAX && arg->value.reg <= GUM_X86_EIP)
          {
            gum_x86_writer_put_mov_reg_reg (self, reg_for_arg_32[arg_index],
                arg->value.reg);
          }
          else
          {
            gum_x86_writer_put_mov_reg_reg (self, reg_for_arg_64[arg_index],
                arg->value.reg);
          }
        }
      }
      else
      {
        if (arg->type == GUM_ARG_ADDRESS)
        {
          gum_x86_writer_put_push_reg (self, GUM_X86_XAX);
          gum_x86_writer_put_mov_reg_address (self, GUM_X86_RAX,
              arg->value.address);
          gum_x86_writer_put_xchg_reg_reg_ptr (self, GUM_X86_RAX, GUM_X86_RSP);
        }
        else
        {
          gum_x86_writer_put_push_reg (self, arg->value.reg);
        }
      }
    }

    if (self->target_abi == GUM_ABI_WINDOWS)
      gum_x86_writer_put_sub_reg_imm (self, GUM_X86_RSP, 4 * 8);
  }
}

static void
gum_x86_writer_put_argument_list_setup_va (GumX86Writer * self,
                                           GumCallingConvention conv,
                                           guint n_args,
                                           va_list args)
{
  GumArgument * arg_values;
  guint arg_index;

  arg_values = g_newa (GumArgument, n_args);

  for (arg_index = 0; arg_index != n_args; arg_index++)
  {
    GumArgument * arg = &arg_values[arg_index];

    arg->type = va_arg (args, GumArgType);
    if (arg->type == GUM_ARG_ADDRESS)
      arg->value.address = va_arg (args, GumAddress);
    else if (arg->type == GUM_ARG_REGISTER)
      arg->value.reg = va_arg (args, GumX86Reg);
    else
      g_assert_not_reached ();
  }

  gum_x86_writer_put_argument_list_setup (self, conv, n_args, arg_values);
}

static void
gum_x86_writer_put_argument_list_teardown (GumX86Writer * self,
                                           GumCallingConvention conv,
                                           guint n_args)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (conv == GUM_CALL_CAPI && n_args != 0)
    {
      gum_x86_writer_put_add_reg_imm (self, GUM_X86_ESP,
          n_args * sizeof (guint32));
    }
  }
  else
  {
    if (self->target_abi == GUM_ABI_WINDOWS)
      gum_x86_writer_put_add_reg_imm (self, GUM_X86_RSP, MAX (n_args, 4) * 8);
    else if (n_args > 6)
      gum_x86_writer_put_add_reg_imm (self, GUM_X86_RSP, (n_args - 6) * 8);
  }
}

static void
gum_x86_writer_put_aligned_argument_list_setup (GumX86Writer * self,
                                                GumCallingConvention conv,
                                                guint n_args,
                                                const GumArgument * args)
{
  guint align_correction;

  align_correction =
      gum_x86_writer_get_needed_alignment_correction (self, n_args);
  if (align_correction != 0)
  {
    gum_x86_writer_put_sub_reg_imm (self, GUM_X86_XSP, align_correction);
  }

  gum_x86_writer_put_argument_list_setup (self, conv, n_args, args);
}

static void
gum_x86_writer_put_aligned_argument_list_setup_va (GumX86Writer * self,
                                                   GumCallingConvention conv,
                                                   guint n_args,
                                                   va_list args)
{
  guint align_correction;

  align_correction =
      gum_x86_writer_get_needed_alignment_correction (self, n_args);
  if (align_correction != 0)
  {
    gum_x86_writer_put_sub_reg_imm (self, GUM_X86_XSP, align_correction);
  }

  gum_x86_writer_put_argument_list_setup_va (self, conv, n_args, args);
}

static void
gum_x86_writer_put_aligned_argument_list_teardown (GumX86Writer * self,
                                                   GumCallingConvention conv,
                                                   guint n_args)
{
  guint align_correction;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  align_correction =
      gum_x86_writer_get_needed_alignment_correction (self, n_args);
  if (align_correction != 0)
  {
    gum_x86_writer_put_add_reg_imm (self, GUM_X86_XSP, align_correction);
  }
}

static guint
gum_x86_writer_get_needed_alignment_correction (GumX86Writer * self,
                                                guint n_args)
{
  guint n_stack_args, pointer_size, stack_args_size, remainder;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    n_stack_args = n_args;

    pointer_size = 4;
  }
  else
  {
    if (self->target_abi == GUM_ABI_UNIX)
      n_stack_args = (n_args > 6) ? n_args - 6 : 0;
    else
      n_stack_args = (n_args > 4) ? n_args - 4 : 0;

    pointer_size = 8;
  }

  stack_args_size = n_stack_args * pointer_size;

  remainder = stack_args_size % 16;

  return (remainder != 0) ? 16 - remainder : 0;
}

gboolean
gum_x86_writer_put_call_reg_offset_ptr_with_arguments (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumX86Reg reg,
    gssize offset,
    guint n_args,
    ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_reg_offset_ptr (self, reg, offset))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_offset_ptr_with_arguments_array (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumX86Reg reg,
    gssize offset,
    guint n_args,
    const GumArgument * args)
{
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_reg_offset_ptr (self, reg, offset))
    return FALSE;

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_offset_ptr_with_aligned_arguments (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumX86Reg reg,
    gssize offset,
    guint n_args,
    ...)
{
  va_list args;

  va_start (args, n_args);
  gum_x86_writer_put_aligned_argument_list_setup_va (self, conv, n_args, args);
  va_end (args);

  if (!gum_x86_writer_put_call_reg_offset_ptr (self, reg, offset))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_offset_ptr_with_aligned_arguments_array (
    GumX86Writer * self,
    GumCallingConvention conv,
    GumX86Reg reg,
    gssize offset,
    guint n_args,
    const GumArgument * args)
{
  gum_x86_writer_put_aligned_argument_list_setup (self, conv, n_args, args);

  if (!gum_x86_writer_put_call_reg_offset_ptr (self, reg, offset))
    return FALSE;

  gum_x86_writer_put_aligned_argument_list_teardown (self, conv, n_args);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_address (GumX86Writer * self,
                                 GumAddress address)
{
  gint64 distance;
  gboolean distance_fits_in_i32;

  distance = (gssize) address - (gssize) (self->pc + 5);
  distance_fits_in_i32 = (distance >= G_MININT32 && distance <= G_MAXINT32);

  if (distance_fits_in_i32)
  {
    self->code[0] = 0xe8;
    *((gint32 *) (self->code + 1)) = GINT32_TO_LE (distance);
    gum_x86_writer_commit (self, 5);
  }
  else
  {
    gconstpointer call_target_storage = self->code + 1;
    gconstpointer carry_on = self->code + 2;

    if (self->target_cpu != GUM_CPU_AMD64)
      return FALSE;

    gum_x86_writer_put_call_indirect_label (self, call_target_storage);
    gum_x86_writer_put_jmp_short_label (self, carry_on);

    gum_x86_writer_put_label (self, call_target_storage);
    *((guint64 *) (self->code)) = GUINT64_TO_LE (address);
    gum_x86_writer_commit (self, 8);

    gum_x86_writer_put_label (self, carry_on);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg (GumX86Writer * self,
                             GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (ri.index_is_extended)
    gum_x86_writer_put_u8 (self, 0x41);
  self->code[0] = 0xff;
  self->code[1] = 0xd0 | ri.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_reg_offset_ptr (GumX86Writer * self,
                                        GumX86Reg reg,
                                        gssize offset)
{
  GumX86RegInfo ri;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (offset);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = (offset_fits_in_i8 ? 0x50 : 0x90) | ri.index;
  gum_x86_writer_commit (self, 2);

  if (ri.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  if (offset_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, offset);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_call_indirect (GumX86Writer * self,
                                  GumAddress address)
{
  if (self->target_cpu == GUM_CPU_AMD64)
  {
    gint64 distance = (gint64) address - (gint64) (self->pc + 6);

    if (!GUM_IS_WITHIN_INT32_RANGE (distance))
      return FALSE;

    self->code[0] = 0xff;
    self->code[1] = 0x15;
    *((guint32 *) (self->code + 2)) = GINT32_TO_LE ((gint32) distance);
  }
  else
  {
    self->code[0] = 0xff;
    self->code[1] = 0x15;
    *((guint32 *) (self->code + 2)) = GUINT32_TO_LE (address);
  }

  gum_x86_writer_commit (self, 6);

  return TRUE;
}

gboolean
gum_x86_writer_put_call_indirect_label (GumX86Writer * self,
                                        gconstpointer label_id)
{
  if (!gum_x86_writer_put_call_indirect (self, self->pc))
    return FALSE;

  gum_x86_writer_add_label_reference_here (self, label_id,
      (self->target_cpu == GUM_CPU_AMD64)
          ? GUM_LREF_NEAR
          : GUM_LREF_ABS);
  return TRUE;
}

void
gum_x86_writer_put_call_near_label (GumX86Writer * self,
                                    gconstpointer label_id)
{
  gum_x86_writer_put_call_address (self, self->pc);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_NEAR);
}

void
gum_x86_writer_put_leave (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xc9);
}

void
gum_x86_writer_put_ret (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xc3);
}

void
gum_x86_writer_put_ret_imm (GumX86Writer * self,
                            guint16 imm_value)
{
  self->code[0] = 0xc2;
  *((guint16 *) (self->code + 1)) = GUINT16_TO_LE (imm_value);
  gum_x86_writer_commit (self, 3);
}

gboolean
gum_x86_writer_put_jmp_address (GumX86Writer * self,
                                GumAddress address)
{
  gint64 distance;

  distance = (gssize) address - (gssize) (self->pc + 2);

  if (GUM_IS_WITHIN_INT8_RANGE (distance))
  {
    self->code[0] = 0xeb;
    *((gint8 *) (self->code + 1)) = distance;
    gum_x86_writer_commit (self, 2);
  }
  else
  {
    distance = (gssize) address - (gssize) (self->pc + 5);

    if (GUM_IS_WITHIN_INT32_RANGE (distance))
    {
      self->code[0] = 0xe9;
      *((gint32 *) (self->code + 1)) = GINT32_TO_LE ((gint32) distance);
      gum_x86_writer_commit (self, 5);
    }
    else
    {
      if (self->target_cpu != GUM_CPU_AMD64)
        return FALSE;

      self->code[0] = 0xff;
      self->code[1] = 0x25;
      *((gint32 *) (self->code + 2)) = GINT32_TO_LE (2); /* RIP + 2 */
      self->code[6] = 0x0f;
      self->code[7] = 0x0b;
      *((guint64 *) (self->code + 8)) = GUINT64_TO_LE (address);
      gum_x86_writer_commit (self, 16);
    }
  }

  return TRUE;
}

static gboolean
gum_x86_writer_put_short_jmp (GumX86Writer * self,
                              gconstpointer target)
{
  gint64 distance;

  distance = (gssize) target - (gssize) (self->pc + 2);
  if (!GUM_IS_WITHIN_INT8_RANGE (distance))
    return FALSE;

  self->code[0] = 0xeb;
  *((gint8 *) (self->code + 1)) = distance;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

static gboolean
gum_x86_writer_put_near_jmp (GumX86Writer * self,
                             gconstpointer target)
{
  gint64 distance;

  distance = (gssize) target - (gssize) (self->pc + 5);

  if (GUM_IS_WITHIN_INT32_RANGE (distance))
  {
    self->code[0] = 0xe9;
    *((gint32 *) (self->code + 1)) = GINT32_TO_LE (distance);
    gum_x86_writer_commit (self, 5);
  }
  else
  {
    if (self->target_cpu != GUM_CPU_AMD64)
      return FALSE;

    self->code[0] = 0xff;                               /* JMP [RIP + 2] */
    self->code[1] = 0x25;
    *((gint32 *) (self->code + 2)) = GINT32_TO_LE (2);  /* RIP + 2 */

    self->code[6] = 0x0f;                               /* UD2 */
    self->code[7] = 0x0b;

    *((guint64 *) (self->code + 8)) = GUINT64_TO_LE (GPOINTER_TO_SIZE (target));
    gum_x86_writer_commit (self, 16);
  }

  return TRUE;
}

void
gum_x86_writer_put_jmp_short_label (GumX86Writer * self,
                                    gconstpointer label_id)
{
  gum_x86_writer_put_short_jmp (self, GSIZE_TO_POINTER (self->pc));
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

void
gum_x86_writer_put_jmp_near_label (GumX86Writer * self,
                                   gconstpointer label_id)
{
  gum_x86_writer_put_near_jmp (self, GSIZE_TO_POINTER (self->pc));
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_NEAR);
}

gboolean
gum_x86_writer_put_jmp_reg (GumX86Writer * self,
                            GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = 0xe0 | ri.index;
  gum_x86_writer_commit (self, 2);

  gum_x86_writer_put_ud2 (self);

  return TRUE;
}

gboolean
gum_x86_writer_put_jmp_reg_ptr (GumX86Writer * self,
                                GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = 0x20 | ri.index;
  gum_x86_writer_commit (self, 2);

  if (ri.meta == GUM_X86_META_XSP)
    gum_x86_writer_put_u8 (self, 0x24);

  gum_x86_writer_put_ud2 (self);

  return TRUE;
}

gboolean
gum_x86_writer_put_jmp_reg_offset_ptr (GumX86Writer * self,
                                       GumX86Reg reg,
                                       gssize offset)
{
  GumX86RegInfo ri;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (offset);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = (offset_fits_in_i8 ? 0x60 : 0xa0) | ri.index;
  gum_x86_writer_commit (self, 2);

  if (ri.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  if (offset_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, offset);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  gum_x86_writer_put_ud2 (self);

  return TRUE;
}

gboolean
gum_x86_writer_put_jmp_near_ptr (GumX86Writer * self,
                                 GumAddress address)
{
  self->code[0] = 0xff;
  self->code[1] = 0x25;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) (self->code + 2)) = GUINT32_TO_LE ((guint32) address);
  }
  else
  {
    gint64 distance = (gint64) address - (gint64) (self->pc + 6);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) (self->code + 2)) = GINT32_TO_LE ((gint32) distance);
  }

  gum_x86_writer_commit (self, 6);

  gum_x86_writer_put_ud2 (self);

  return TRUE;
}

/*
 * This instruction causes a UD exception when executed, which isn't very
 * useful, however its presence also stalls the branch predictor. e.g. if the
 * CPU encounters a `JMP [reg]` instruction, and there is no entry in its branch
 * target buffer (cache of previous branches) it will assume that execution
 * continues with the next instruction (which is where compilers will typically
 * place the most common branch of a switch statement). However, in most cases
 * (e.g. Stalker) such indirect branches will typically be used to divert
 * control flow to an address which can only be determined at runtime. As such
 * by following these branches with `UD2`, we can prevent the speculative
 * execution of subsequent instructions and hence the overhead of unwinding
 * them.
 */
static void
gum_x86_writer_put_ud2 (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x0f);
  gum_x86_writer_put_u8 (self, 0x0b);
}

gboolean
gum_x86_writer_put_jcc_short (GumX86Writer * self,
                              x86_insn instruction_id,
                              gconstpointer target,
                              GumBranchHint hint)
{
  gssize distance;

  if (hint != GUM_NO_HINT)
    gum_x86_writer_put_u8 (self, (hint == GUM_LIKELY) ? 0x3e : 0x2e);
  self->code[0] = gum_get_jcc_opcode (instruction_id);
  distance = (gssize) target - (gssize) (self->pc + 2);
  if (!GUM_IS_WITHIN_INT8_RANGE (distance))
    return FALSE;
  *((gint8 *) (self->code + 1)) = distance;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_jcc_near (GumX86Writer * self,
                             x86_insn instruction_id,
                             gconstpointer target,
                             GumBranchHint hint)
{
  gssize distance;

  if (hint != GUM_NO_HINT)
    gum_x86_writer_put_u8 (self, (hint == GUM_LIKELY) ? 0x3e : 0x2e);
  self->code[0] = 0x0f;
  self->code[1] = 0x10 + gum_get_jcc_opcode (instruction_id);
  distance = (gssize) target - (gssize) (self->pc + 6);
  if (!GUM_IS_WITHIN_INT32_RANGE (distance))
    return FALSE;
  *((gint32 *) (self->code + 2)) = GINT32_TO_LE (distance);
  gum_x86_writer_commit (self, 6);

  return TRUE;
}

void
gum_x86_writer_put_jcc_short_label (GumX86Writer * self,
                                    x86_insn instruction_id,
                                    gconstpointer label_id,
                                    GumBranchHint hint)
{
  gum_x86_writer_put_jcc_short (self, instruction_id,
      GSIZE_TO_POINTER (self->pc), hint);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

void
gum_x86_writer_put_jcc_near_label (GumX86Writer * self,
                                   x86_insn instruction_id,
                                   gconstpointer label_id,
                                   GumBranchHint hint)
{
  gum_x86_writer_put_jcc_near (self, instruction_id,
      GSIZE_TO_POINTER (self->pc), hint);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_NEAR);
}

static gboolean
gum_x86_writer_put_add_or_sub_reg_imm (GumX86Writer * self,
                                       GumX86Reg reg,
                                       gssize imm_value,
                                       gboolean add)
{
  GumX86RegInfo ri;
  gboolean immediate_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  immediate_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (imm_value);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  if (ri.meta == GUM_X86_META_XAX && !immediate_fits_in_i8)
  {
    gum_x86_writer_put_u8 (self, add ? 0x05 : 0x2d);
  }
  else
  {
    self->code[0] = immediate_fits_in_i8 ? 0x83 : 0x81;
    self->code[1] = (add ? 0xc0 : 0xe8) | ri.index;
    gum_x86_writer_commit (self, 2);
  }

  if (immediate_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, imm_value);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (imm_value);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_add_reg_imm (GumX86Writer * self,
                                GumX86Reg reg,
                                gssize imm_value)
{
  return gum_x86_writer_put_add_or_sub_reg_imm (self, reg, imm_value, TRUE);
}

gboolean
gum_x86_writer_put_add_reg_reg (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (src.width != dst.width)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, &src,
      NULL))
    return FALSE;

  self->code[0] = 0x01;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_add_reg_near_ptr (GumX86Writer * self,
                                     GumX86Reg dst_reg,
                                     GumAddress src_address)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, NULL))
    return FALSE;

  self->code[0] = 0x03;
  self->code[1] = 0x05 | (dst.index << 3);
  gum_x86_writer_commit (self, 2);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (src_address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) self->code) = GUINT32_TO_LE ((guint32) src_address);
  }
  else
  {
    gint64 distance = (gint64) src_address - (gint64) (self->pc + 4);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) self->code) = GINT32_TO_LE ((gint32) distance);
  }
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

gboolean
gum_x86_writer_put_sub_reg_imm (GumX86Writer * self,
                                GumX86Reg reg,
                                gssize imm_value)
{
  return gum_x86_writer_put_add_or_sub_reg_imm (self, reg, imm_value, FALSE);
}

gboolean
gum_x86_writer_put_sub_reg_reg (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (src.width != dst.width)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, &src,
      NULL))
    return FALSE;

  self->code[0] = 0x29;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_sub_reg_near_ptr (GumX86Writer * self,
                                     GumX86Reg dst_reg,
                                     GumAddress src_address)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, NULL))
    return FALSE;

  self->code[0] = 0x2b;
  self->code[1] = 0x05 | (dst.index << 3);
  gum_x86_writer_commit (self, 2);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (src_address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) self->code) = GUINT32_TO_LE ((guint32) src_address);
  }
  else
  {
    gint64 distance = (gint64) src_address - (gint64) (self->pc + 4);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) self->code) = GINT32_TO_LE ((gint32) distance);
  }
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

gboolean
gum_x86_writer_put_inc_reg (GumX86Writer * self,
                            GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu != GUM_CPU_AMD64 &&
      (ri.width != 32 || ri.index_is_extended))
  {
    return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = 0xc0 | ri.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_dec_reg (GumX86Writer * self,
                            GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu != GUM_CPU_AMD64 &&
      (ri.width != 32 || ri.index_is_extended))
  {
    return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  self->code[0] = 0xff;
  self->code[1] = 0xc8 | ri.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

static gboolean
gum_x86_writer_put_inc_or_dec_reg_ptr (GumX86Writer * self,
                                       GumX86PtrTarget target,
                                       GumX86Reg reg,
                                       gboolean increment)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (target == GUM_X86_PTR_QWORD)
      gum_x86_writer_put_u8 (self, 0x48 | (ri.index_is_extended ? 0x01 : 0x00));
    else if (ri.index_is_extended)
      gum_x86_writer_put_u8 (self, 0x41);
  }

  switch (target)
  {
    case GUM_X86_PTR_BYTE:
      gum_x86_writer_put_u8 (self, 0xfe);
      break;
    case GUM_X86_PTR_QWORD:
      if (self->target_cpu != GUM_CPU_AMD64)
        return FALSE;
    case GUM_X86_PTR_DWORD:
      gum_x86_writer_put_u8 (self, 0xff);
      break;
  }

  gum_x86_writer_put_u8 (self, (increment ? 0x00 : 0x08) | ri.index);

  return TRUE;
}

gboolean
gum_x86_writer_put_inc_reg_ptr (GumX86Writer * self,
                                GumX86PtrTarget target,
                                GumX86Reg reg)
{
  return gum_x86_writer_put_inc_or_dec_reg_ptr (self, target, reg, TRUE);
}

gboolean
gum_x86_writer_put_dec_reg_ptr (GumX86Writer * self,
                                GumX86PtrTarget target,
                                GumX86Reg reg)
{
  return gum_x86_writer_put_inc_or_dec_reg_ptr (self, target, reg, FALSE);
}

gboolean
gum_x86_writer_put_lock_xadd_reg_ptr_reg (GumX86Writer * self,
                                          GumX86Reg dst_reg,
                                          GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  gum_x86_writer_put_u8 (self, 0xf0); /* lock prefix */

  if (!gum_x86_writer_put_prefix_for_registers (self, &src, 32, &dst, &src,
      NULL))
    return FALSE;

  self->code[0] = 0x0f;
  self->code[1] = 0xc1;
  self->code[2] = 0x00 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 3);

  if (dst.meta == GUM_X86_META_XSP)
  {
    gum_x86_writer_put_u8 (self, 0x24);
  }
  else if (dst.meta == GUM_X86_META_XBP)
  {
    self->code[-1] |= 0x40;
    gum_x86_writer_put_u8 (self, 0x00);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_lock_cmpxchg_reg_ptr_reg (GumX86Writer * self,
                                             GumX86Reg dst_reg,
                                             GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (dst.width != 32)
      return FALSE;
  }
  else
  {
    if (dst.width != 64)
      return FALSE;
  }
  if (dst.index_is_extended)
    return FALSE;
  if (src.width != 32 || src.index_is_extended)
    return FALSE;

  self->code[0] = 0xf0; /* lock prefix */
  self->code[1] = 0x0f;
  self->code[2] = 0xb1;
  self->code[3] = 0x00 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 4);

  if (dst.meta == GUM_X86_META_XSP)
  {
    gum_x86_writer_put_u8 (self, 0x24);
  }
  else if (dst.meta == GUM_X86_META_XBP)
  {
    self->code[-1] |= 0x40;
    gum_x86_writer_put_u8 (self, 0x00);
  }

  return TRUE;
}

static gboolean
gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (GumX86Writer * self,
                                              gpointer target,
                                              gboolean increment)
{
  self->code[0] = 0xf0;
  self->code[1] = 0xff;
  self->code[2] = increment ? 0x05 : 0x0d;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    *((guint32 *) (self->code + 3)) = GUINT32_TO_LE (GPOINTER_TO_SIZE (target));
  }
  else
  {
    gint64 distance = (gssize) target - (gssize) (self->pc + 7);
    if (!GUM_IS_WITHIN_INT32_RANGE (distance))
      return FALSE;
    *((gint32 *) (self->code + 3)) = GINT32_TO_LE (distance);
  }

  gum_x86_writer_commit (self, 7);

  return TRUE;
}

gboolean
gum_x86_writer_put_lock_inc_imm32_ptr (GumX86Writer * self,
                                       gpointer target)
{
  return gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (self, target, TRUE);
}

gboolean
gum_x86_writer_put_lock_dec_imm32_ptr (GumX86Writer * self,
                                       gpointer target)
{
  return gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (self, target, FALSE);
}

gboolean
gum_x86_writer_put_and_reg_reg (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (dst.width != src.width)
    return FALSE;
  if (dst.index_is_extended || src.index_is_extended)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0))
    return FALSE;

  self->code[0] = 0x21;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_and_reg_u32 (GumX86Writer * self,
                                GumX86Reg reg,
                                guint32 imm_value)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  if (ri.meta == GUM_X86_META_XAX)
  {
    self->code[0] = 0x25;
    *((guint32 *) (self->code + 1)) = GUINT32_TO_LE (imm_value);
    gum_x86_writer_commit (self, 5);
  }
  else
  {
    self->code[0] = 0x81;
    self->code[1] = 0xe0 | ri.index;
    *((guint32 *) (self->code + 2)) = GUINT32_TO_LE (imm_value);
    gum_x86_writer_commit (self, 6);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_shl_reg_u8 (GumX86Writer * self,
                               GumX86Reg reg,
                               guint8 imm_value)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  self->code[0] = 0xc1;
  self->code[1] = 0xe0 | ri.index;
  self->code[2] = imm_value;
  gum_x86_writer_commit (self, 3);

  return TRUE;
}

gboolean
gum_x86_writer_put_shr_reg_u8 (GumX86Writer * self,
                               GumX86Reg reg,
                               guint8 imm_value)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  self->code[0] = 0xc1;
  self->code[1] = 0xe8 | ri.index;
  self->code[2] = imm_value;
  gum_x86_writer_commit (self, 3);

  return TRUE;
}

gboolean
gum_x86_writer_put_xor_reg_reg (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (dst.width != src.width)
    return FALSE;
  if (dst.index_is_extended || src.index_is_extended)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0))
    return FALSE;

  self->code[0] = 0x31;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_reg_reg (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (dst.width != src.width)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, &src,
      NULL))
    return FALSE;

  self->code[0] = 0x89;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_reg_u32 (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                guint32 imm_value)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (dst.width != 32)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0))
    return FALSE;

  self->code[0] = 0xb8 | dst.index;
  *((guint32 *) (self->code + 1)) = GUINT32_TO_LE (imm_value);
  gum_x86_writer_commit (self, 5);

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_reg_u64 (GumX86Writer * self,
                                GumX86Reg dst_reg,
                                guint64 imm_value)
{
  GumX86RegInfo dst;

  if (self->target_cpu != GUM_CPU_AMD64)
    return FALSE;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (dst.width != 64)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0))
    return FALSE;

  self->code[0] = 0xb8 | dst.index;
  *((guint64 *) (self->code + 1)) = GUINT64_TO_LE (imm_value);
  gum_x86_writer_commit (self, 9);

  return TRUE;
}

void
gum_x86_writer_put_mov_reg_address (GumX86Writer * self,
                                    GumX86Reg dst_reg,
                                    GumAddress address)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (dst.width == 32)
    gum_x86_writer_put_mov_reg_u32 (self, dst_reg, (guint32) address);
  else
    gum_x86_writer_put_mov_reg_u64 (self, dst_reg, (guint64) address);
}

void
gum_x86_writer_put_mov_reg_ptr_u32 (GumX86Writer * self,
                                    GumX86Reg dst_reg,
                                    guint32 imm_value)
{
  gum_x86_writer_put_mov_reg_offset_ptr_u32 (self, dst_reg, 0, imm_value);
}

gboolean
gum_x86_writer_put_mov_reg_offset_ptr_u32 (GumX86Writer * self,
                                           GumX86Reg dst_reg,
                                           gssize dst_offset,
                                           guint32 imm_value)
{
  GumX86RegInfo dst;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (dst.width != 32)
      return FALSE;
  }
  else
  {
    if (dst.width != 64)
      return FALSE;
  }

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (dst_offset);

  gum_x86_writer_put_u8 (self, 0xc7);

  if (dst_offset == 0 && dst.meta != GUM_X86_META_XBP)
  {
    gum_x86_writer_put_u8 (self, 0x00 | dst.index);
    if (dst.meta == GUM_X86_META_XSP)
      gum_x86_writer_put_u8 (self, 0x24);
  }
  else
  {
    gum_x86_writer_put_u8 (self,
        (offset_fits_in_i8 ? 0x40 : 0x80) | dst.index);

    if (dst.meta == GUM_X86_META_XSP)
      gum_x86_writer_put_u8 (self, 0x24);

    if (offset_fits_in_i8)
    {
      gum_x86_writer_put_u8 (self, dst_offset);
    }
    else
    {
      *((gint32 *) self->code) = GINT32_TO_LE (dst_offset);
      gum_x86_writer_commit (self, 4);
    }
  }

  *((guint32 *) self->code) = GUINT32_TO_LE (imm_value);
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

void
gum_x86_writer_put_mov_reg_ptr_reg (GumX86Writer * self,
                                    GumX86Reg dst_reg,
                                    GumX86Reg src_reg)
{
  gum_x86_writer_put_mov_reg_offset_ptr_reg (self, dst_reg, 0, src_reg);
}

gboolean
gum_x86_writer_put_mov_reg_offset_ptr_reg (GumX86Writer * self,
                                           GumX86Reg dst_reg,
                                           gssize dst_offset,
                                           GumX86Reg src_reg)
{
  GumX86RegInfo dst, src;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (dst.width != 32 || src.width != 32)
      return FALSE;
  }
  else
  {
    if (dst.width != 64)
      return FALSE;
  }

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (dst_offset);

  if (!gum_x86_writer_put_prefix_for_registers (self, &src, 32, &dst, &src,
      NULL))
    return FALSE;

  gum_x86_writer_put_u8 (self, 0x89);

  if (dst_offset == 0 && dst.meta != GUM_X86_META_XBP)
  {
    gum_x86_writer_put_u8 (self, 0x00 | (src.index << 3) | dst.index);
    if (dst.meta == GUM_X86_META_XSP)
      gum_x86_writer_put_u8 (self, 0x24);
  }
  else
  {
    gum_x86_writer_put_u8 (self, (offset_fits_in_i8 ? 0x40 : 0x80) |
        (src.index << 3) | dst.index);

    if (dst.meta == GUM_X86_META_XSP)
      gum_x86_writer_put_u8 (self, 0x24);

    if (offset_fits_in_i8)
    {
      gum_x86_writer_put_s8 (self, dst_offset);
    }
    else
    {
      *((gint32 *) self->code) = GINT32_TO_LE (dst_offset);
      gum_x86_writer_commit (self, 4);
    }
  }

  return TRUE;
}

void
gum_x86_writer_put_mov_reg_reg_ptr (GumX86Writer * self,
                                    GumX86Reg dst_reg,
                                    GumX86Reg src_reg)
{
  gum_x86_writer_put_mov_reg_reg_offset_ptr (self, dst_reg, src_reg, 0);
}

gboolean
gum_x86_writer_put_mov_reg_reg_offset_ptr (GumX86Writer * self,
                                           GumX86Reg dst_reg,
                                           GumX86Reg src_reg,
                                           gssize src_offset)
{
  GumX86RegInfo dst, src;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (dst.width != 32 || src.width != 32)
      return FALSE;
  }
  else
  {
    if (src.width != 64)
      return FALSE;
  }

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (src_offset);

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &src, &dst,
      NULL))
    return FALSE;

  self->code[0] = 0x8b;
  self->code[1] = ((offset_fits_in_i8) ? 0x40 : 0x80)
      | (dst.index << 3) | src.index;
  gum_x86_writer_commit (self, 2);

  if (src.meta == GUM_X86_META_XSP)
    gum_x86_writer_put_u8 (self, 0x24);

  if (offset_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, src_offset);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (src_offset);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_reg_base_index_scale_offset_ptr (GumX86Writer * self,
                                                        GumX86Reg dst_reg,
                                                        GumX86Reg base_reg,
                                                        GumX86Reg index_reg,
                                                        guint8 scale,
                                                        gssize offset)
{
  GumX86RegInfo dst, base, index;
  gboolean offset_fits_in_i8;
  const guint8 scale_lookup[] = {
      /* 0: */ 0xff,
      /* 1: */    0,
      /* 2: */    1,
      /* 3: */ 0xff,
      /* 4: */    2,
      /* 5: */ 0xff,
      /* 6: */ 0xff,
      /* 7: */ 0xff,
      /* 8: */    3
  };

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, base_reg, &base);
  gum_x86_writer_describe_cpu_reg (self, index_reg, &index);

  if (dst.index_is_extended)
    return FALSE;
  if (base.width != index.width)
    return FALSE;
  if (base.index_is_extended || index.index_is_extended)
    return FALSE;
  if (index.meta == GUM_X86_META_XSP)
    return FALSE;
  if (scale != 1 && scale != 2 && scale != 4 && scale != 8)
    return FALSE;

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (offset);

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (dst.width != 64 || base.width != 64 || index.width != 64)
      return FALSE;

    gum_x86_writer_put_u8 (self, 0x48);
  }

  self->code[0] = 0x8b;
  self->code[1] = (offset_fits_in_i8 ? 0x40 : 0x80) | (dst.index << 3) | 0x04;
  self->code[2] = (scale_lookup[scale] << 6) | (index.index << 3) | base.index;
  gum_x86_writer_commit (self, 3);

  if (offset_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, offset);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_reg_near_ptr (GumX86Writer * self,
                                     GumX86Reg dst_reg,
                                     GumAddress src_address)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, NULL))
    return FALSE;

  if (self->target_cpu == GUM_CPU_IA32 && dst.meta == GUM_X86_META_XAX)
  {
    gum_x86_writer_put_u8 (self, 0xa1);
  }
  else
  {
    self->code[0] = 0x8b;
    self->code[1] = (dst.index << 3) | 0x05;
    gum_x86_writer_commit (self, 2);
  }

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (src_address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) self->code) = GUINT32_TO_LE ((guint32) src_address);
  }
  else
  {
    gint64 distance = (gint64) src_address - (gint64) (self->pc + 4);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) self->code) = GINT32_TO_LE ((gint32) distance);
  }
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_near_ptr_reg (GumX86Writer * self,
                                     GumAddress dst_address,
                                     GumX86Reg src_reg)
{
  GumX86RegInfo src;

  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (!gum_x86_writer_put_prefix_for_registers (self, &src, 32, &src, NULL))
    return FALSE;

  if (self->target_cpu == GUM_CPU_IA32 && src.meta == GUM_X86_META_XAX)
  {
    gum_x86_writer_put_u8 (self, 0xa3);
  }
  else
  {
    self->code[0] = 0x89;
    self->code[1] = (src.index << 3) | 0x05;
    gum_x86_writer_commit (self, 2);
  }

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (dst_address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) self->code) = GUINT32_TO_LE ((guint32) dst_address);
  }
  else
  {
    gint64 distance = (gint64) dst_address - (gint64) (self->pc + 4);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) self->code) = GINT32_TO_LE ((gint32) distance);
  }
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

static gboolean
gum_x86_writer_put_mov_reg_imm_ptr (GumX86Writer * self,
                                    GumX86Reg dst_reg,
                                    guint32 address)
{
  GumX86RegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (!gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, NULL))
    return FALSE;

  self->code[0] = 0x8b;
  self->code[1] = (dst.index << 3) | 0x04;
  self->code[2] = 0x25;
  *((guint32 *) (self->code + 3)) = GUINT32_TO_LE (address);
  gum_x86_writer_commit (self, 7);

  return TRUE;
}

static gboolean
gum_x86_writer_put_mov_imm_ptr_reg (GumX86Writer * self,
                                    guint32 address,
                                    GumX86Reg src_reg)
{
  GumX86RegInfo src;

  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (!gum_x86_writer_put_prefix_for_registers (self, &src, 32, &src, NULL))
    return FALSE;

  self->code[0] = 0x89;
  self->code[1] = (src.index << 3) | 0x04;
  self->code[2] = 0x25;
  *((guint32 *) (self->code + 3)) = GUINT32_TO_LE (address);
  gum_x86_writer_commit (self, 7);

  return TRUE;
}

gboolean
gum_x86_writer_put_mov_fs_u32_ptr_reg (GumX86Writer * self,
                                       guint32 fs_offset,
                                       GumX86Reg src_reg)
{
  gum_x86_writer_put_u8 (self, 0x64);
  return gum_x86_writer_put_mov_imm_ptr_reg (self, fs_offset, src_reg);
}

gboolean
gum_x86_writer_put_mov_reg_fs_u32_ptr (GumX86Writer * self,
                                       GumX86Reg dst_reg,
                                       guint32 fs_offset)
{
  gum_x86_writer_put_u8 (self, 0x64);
  return gum_x86_writer_put_mov_reg_imm_ptr (self, dst_reg, fs_offset);
}

void
gum_x86_writer_put_mov_fs_reg_ptr_reg (GumX86Writer * self,
                                       GumX86Reg fs_offset,
                                       GumX86Reg src_reg)
{
  gum_x86_writer_put_u8 (self, 0x65);
  gum_x86_writer_put_mov_reg_ptr_reg (self, fs_offset, src_reg);
}

void
gum_x86_writer_put_mov_reg_fs_reg_ptr (GumX86Writer * self,
                                       GumX86Reg dst_reg,
                                       GumX86Reg fs_offset)
{
  gum_x86_writer_put_u8 (self, 0x65);
  gum_x86_writer_put_mov_reg_reg_ptr (self, dst_reg, fs_offset);
}

gboolean
gum_x86_writer_put_mov_gs_u32_ptr_reg (GumX86Writer * self,
                                       guint32 fs_offset,
                                       GumX86Reg src_reg)
{
  gum_x86_writer_put_u8 (self, 0x65);
  return gum_x86_writer_put_mov_imm_ptr_reg (self, fs_offset, src_reg);
}

gboolean
gum_x86_writer_put_mov_reg_gs_u32_ptr (GumX86Writer * self,
                                       GumX86Reg dst_reg,
                                       guint32 fs_offset)
{
  gum_x86_writer_put_u8 (self, 0x65);
  return gum_x86_writer_put_mov_reg_imm_ptr (self, dst_reg, fs_offset);
}

void
gum_x86_writer_put_mov_gs_reg_ptr_reg (GumX86Writer * self,
                                       GumX86Reg gs_offset,
                                       GumX86Reg src_reg)
{
  gum_x86_writer_put_u8 (self, 0x65);
  gum_x86_writer_put_mov_reg_ptr_reg (self, gs_offset, src_reg);
}

void
gum_x86_writer_put_mov_reg_gs_reg_ptr (GumX86Writer * self,
                                       GumX86Reg dst_reg,
                                       GumX86Reg gs_offset)
{
  gum_x86_writer_put_u8 (self, 0x65);
  gum_x86_writer_put_mov_reg_reg_ptr (self, dst_reg, gs_offset);
}

void
gum_x86_writer_put_movq_xmm0_esp_offset_ptr (GumX86Writer * self,
                                             gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x7e;
  self->code[3] = 0x44;
  self->code[4] = 0x24;
  self->code[5] = offset;
  gum_x86_writer_commit (self, 6);
}

void
gum_x86_writer_put_movq_eax_offset_ptr_xmm0 (GumX86Writer * self,
                                             gint8 offset)
{
  self->code[0] = 0x66;
  self->code[1] = 0x0f;
  self->code[2] = 0xd6;
  self->code[3] = 0x40;
  self->code[4] = offset;
  gum_x86_writer_commit (self, 5);
}

void
gum_x86_writer_put_movdqu_xmm0_esp_offset_ptr (GumX86Writer * self,
                                               gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x6f;
  self->code[3] = 0x44;
  self->code[4] = 0x24;
  self->code[5] = offset;
  gum_x86_writer_commit (self, 6);
}

void
gum_x86_writer_put_movdqu_eax_offset_ptr_xmm0 (GumX86Writer * self,
                                               gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x7f;
  self->code[3] = 0x40;
  self->code[4] = offset;
  gum_x86_writer_commit (self, 5);
}

gboolean
gum_x86_writer_put_lea_reg_reg_offset (GumX86Writer * self,
                                       GumX86Reg dst_reg,
                                       GumX86Reg src_reg,
                                       gssize src_offset)
{
  GumX86RegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (dst.index_is_extended || src.index_is_extended)
    return FALSE;

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (src.width == 32)
      gum_x86_writer_put_u8 (self, 0x67);
    if (dst.width == 64)
      gum_x86_writer_put_u8 (self, 0x48);
  }

  self->code[0] = 0x8d;
  self->code[1] = 0x80 | (dst.index << 3) | src.index;
  gum_x86_writer_commit (self, 2);

  if (src.meta == GUM_X86_META_XSP)
    gum_x86_writer_put_u8 (self, 0x24);

  *((gint32 *) self->code) = GINT32_TO_LE (src_offset);
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

gboolean
gum_x86_writer_put_xchg_reg_reg_ptr (GumX86Writer * self,
                                     GumX86Reg left_reg,
                                     GumX86Reg right_reg)
{
  GumX86RegInfo left, right;

  gum_x86_writer_describe_cpu_reg (self, left_reg, &left);
  gum_x86_writer_describe_cpu_reg (self, right_reg, &right);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (right.width != 32)
      return FALSE;
  }
  else
  {
    if (right.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_reg_info (self, &left, 1))
    return FALSE;

  self->code[0] = 0x87;
  self->code[1] = 0x00 | (left.index << 3) | right.index;
  gum_x86_writer_commit (self, 2);

  if (right.meta == GUM_X86_META_XSP)
  {
    gum_x86_writer_put_u8 (self, 0x24);
  }
  else if (right.meta == GUM_X86_META_XBP)
  {
    self->code[-1] |= 0x40;
    gum_x86_writer_put_u8 (self, 0x00);
  }

  return TRUE;
}

void
gum_x86_writer_put_push_u32 (GumX86Writer * self,
                             guint32 imm_value)
{
  self->code[0] = 0x68;
  *((guint32 *) (self->code + 1)) = GUINT32_TO_LE (imm_value);
  gum_x86_writer_commit (self, 5);
}

gboolean
gum_x86_writer_put_push_near_ptr (GumX86Writer * self,
                                  GumAddress address)
{
  self->code[0] = 0xff;
  self->code[1] = 0x35;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (address > G_MAXUINT32)
      return FALSE;
    *((guint32 *) (self->code + 2)) = GUINT32_TO_LE ((guint32) address);
  }
  else
  {
    gint64 distance = (gint64) address - (gint64) (self->pc + 6);
    if (distance < G_MININT32 || distance > G_MAXINT32)
      return FALSE;
    *((gint32 *) (self->code + 2)) = GINT32_TO_LE ((gint32) distance);
  }

  gum_x86_writer_commit (self, 6);

  return TRUE;
}

gboolean
gum_x86_writer_put_push_reg (GumX86Writer * self,
                             GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  gum_x86_writer_put_u8 (self, 0x50 | ri.index);

  return TRUE;
}

gboolean
gum_x86_writer_put_pop_reg (GumX86Writer * self,
                            GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  gum_x86_writer_put_u8 (self, 0x58 | ri.index);

  return TRUE;
}

void
gum_x86_writer_put_push_imm_ptr (GumX86Writer * self,
                                 gconstpointer imm_ptr)
{
  self->code[0] = 0xff;
  self->code[1] = 0x35;
  *((guint32 *) (self->code + 2)) = GUINT32_TO_LE (GUM_ADDRESS (imm_ptr));
  gum_x86_writer_commit (self, 6);
}

void
gum_x86_writer_put_pushax (GumX86Writer * self)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    gum_x86_writer_put_u8 (self, 0x60);
  }
  else
  {
    gum_x86_writer_put_push_reg (self, GUM_X86_RAX);
    gum_x86_writer_put_push_reg (self, GUM_X86_RCX);
    gum_x86_writer_put_push_reg (self, GUM_X86_RDX);
    gum_x86_writer_put_push_reg (self, GUM_X86_RBX);

    gum_x86_writer_put_lea_reg_reg_offset (self, GUM_X86_RAX,
        GUM_X86_RSP, 4 * 8);
    gum_x86_writer_put_push_reg (self, GUM_X86_RAX);
    gum_x86_writer_put_mov_reg_reg_offset_ptr (self, GUM_X86_RAX,
        GUM_X86_RSP, 4 * 8);

    gum_x86_writer_put_push_reg (self, GUM_X86_RBP);
    gum_x86_writer_put_push_reg (self, GUM_X86_RSI);
    gum_x86_writer_put_push_reg (self, GUM_X86_RDI);

    gum_x86_writer_put_push_reg (self, GUM_X86_R8);
    gum_x86_writer_put_push_reg (self, GUM_X86_R9);
    gum_x86_writer_put_push_reg (self, GUM_X86_R10);
    gum_x86_writer_put_push_reg (self, GUM_X86_R11);
    gum_x86_writer_put_push_reg (self, GUM_X86_R12);
    gum_x86_writer_put_push_reg (self, GUM_X86_R13);
    gum_x86_writer_put_push_reg (self, GUM_X86_R14);
    gum_x86_writer_put_push_reg (self, GUM_X86_R15);
  }
}

void
gum_x86_writer_put_popax (GumX86Writer * self)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    gum_x86_writer_put_u8 (self, 0x61);
  }
  else
  {
    gum_x86_writer_put_pop_reg (self, GUM_X86_R15);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R14);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R13);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R12);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R11);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R10);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R9);
    gum_x86_writer_put_pop_reg (self, GUM_X86_R8);

    gum_x86_writer_put_pop_reg (self, GUM_X86_RDI);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RSI);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RBP);
    gum_x86_writer_put_lea_reg_reg_offset (self, GUM_X86_RSP, GUM_X86_RSP, 8);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RBX);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RDX);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RCX);
    gum_x86_writer_put_pop_reg (self, GUM_X86_RAX);
  }
}

void
gum_x86_writer_put_pushfx (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x9c);
}

void
gum_x86_writer_put_popfx (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x9d);
}

void
gum_x86_writer_put_sahf (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x9e);
}

void
gum_x86_writer_put_lahf (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x9f);
}

gboolean
gum_x86_writer_put_test_reg_reg (GumX86Writer * self,
                                 GumX86Reg reg_a,
                                 GumX86Reg reg_b)
{
  GumX86RegInfo a, b;

  gum_x86_writer_describe_cpu_reg (self, reg_a, &a);
  gum_x86_writer_describe_cpu_reg (self, reg_b, &b);

  if (a.width != b.width)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_registers (self, &a, 32, &a, &b, NULL))
    return FALSE;

  self->code[0] = 0x85;
  self->code[1] = 0xc0 | (b.index << 3) | a.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

gboolean
gum_x86_writer_put_test_reg_u32 (GumX86Writer * self,
                                 GumX86Reg reg,
                                 guint32 imm_value)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  if (ri.meta == GUM_X86_META_XAX)
  {
    self->code[0] = 0xa9;
    *((guint32 *) (self->code + 1)) = GUINT32_TO_LE (imm_value);
    gum_x86_writer_commit (self, 5);
  }
  else
  {
    self->code[0] = 0xf7;
    self->code[1] = 0xc0 | ri.index;
    *((guint32 *) (self->code + 2)) = GUINT32_TO_LE (imm_value);
    gum_x86_writer_commit (self, 6);
  }

  return TRUE;
}

gboolean
gum_x86_writer_put_cmp_reg_i32 (GumX86Writer * self,
                                GumX86Reg reg,
                                gint32 imm_value)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL))
    return FALSE;

  if (ri.meta == GUM_X86_META_XAX)
  {
    gum_x86_writer_put_u8 (self, 0x3d);
  }
  else
  {
    self->code[0] = 0x81;
    self->code[1] = 0xf8 | ri.index;
    gum_x86_writer_commit (self, 2);
  }

  *((gint32 *) self->code) = GINT32_TO_LE (imm_value);
  gum_x86_writer_commit (self, 4);

  return TRUE;
}

gboolean
gum_x86_writer_put_cmp_reg_offset_ptr_reg (GumX86Writer * self,
                                           GumX86Reg reg_a,
                                           gssize offset,
                                           GumX86Reg reg_b)
{
  GumX86RegInfo a, b;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg_a, &a);
  gum_x86_writer_describe_cpu_reg (self, reg_b, &b);

  if (!gum_x86_writer_put_prefix_for_registers (self, &a, 32, &a, &b, NULL))
    return FALSE;

  offset_fits_in_i8 = GUM_IS_WITHIN_INT8_RANGE (offset);

  self->code[0] = 0x39;
  self->code[1] = (offset_fits_in_i8 ? 0x40 : 0x80) | (b.index << 3) | a.index;
  gum_x86_writer_commit (self, 2);

  if (a.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  if (offset_fits_in_i8)
  {
    gum_x86_writer_put_s8 (self, offset);
  }
  else
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

void
gum_x86_writer_put_cmp_imm_ptr_imm_u32 (GumX86Writer * self,
                                        gconstpointer imm_ptr,
                                        guint32 imm_value)
{
  self->code[0] = 0x81;
  self->code[1] = 0x3d;
  *((guint32 *) (self->code + 2)) = GUINT32_TO_LE (GUM_ADDRESS (imm_ptr));
  *((guint32 *) (self->code + 6)) = GUINT32_TO_LE (imm_value);
  gum_x86_writer_commit (self, 10);
}

gboolean
gum_x86_writer_put_cmp_reg_reg (GumX86Writer * self,
                                GumX86Reg reg_a,
                                GumX86Reg reg_b)
{
  GumX86RegInfo a, b;

  gum_x86_writer_describe_cpu_reg (self, reg_a, &a);
  gum_x86_writer_describe_cpu_reg (self, reg_b, &b);

  if (a.width != b.width)
    return FALSE;

  if (!gum_x86_writer_put_prefix_for_registers (self, &a, 32, &a, &b, NULL))
    return FALSE;

  self->code[0] = 0x39;
  self->code[1] = 0xc0 | (b.index << 3) | a.index;
  gum_x86_writer_commit (self, 2);

  return TRUE;
}

void
gum_x86_writer_put_clc (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xf8);
}

void
gum_x86_writer_put_stc (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xf9);
}

void
gum_x86_writer_put_cld (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xfc);
}

void
gum_x86_writer_put_std (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xfd);
}

void
gum_x86_writer_put_cpuid (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0xa2;
  gum_x86_writer_commit (self, 2);
}

void
gum_x86_writer_put_lfence (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0xae;
  self->code[2] = 0xe8;
  gum_x86_writer_commit (self, 3);
}

void
gum_x86_writer_put_rdtsc (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0x31;
  gum_x86_writer_commit (self, 2);
}

void
gum_x86_writer_put_pause (GumX86Writer * self)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x90;
  gum_x86_writer_commit (self, 2);
}

void
gum_x86_writer_put_nop (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0x90);
}

void
gum_x86_writer_put_endbr (GumX86Writer * self)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x1e;
  self->code[3] = (self->target_cpu == GUM_CPU_AMD64) ? 0xfa : 0xfb;
  gum_x86_writer_commit (self, 4);
}

void
gum_x86_writer_put_breakpoint (GumX86Writer * self)
{
  gum_x86_writer_put_u8 (self, 0xcc);
}

void
gum_x86_writer_put_padding (GumX86Writer * self,
                            guint n)
{
  gum_memset (self->code, 0xcc, n);
  gum_x86_writer_commit (self, n);
}

/*
 * Whilst the 0x90 opcode for NOP is commonly known, the Intel Optimization
 * Manual actually lists a number of different NOP instructions ranging from
 * one to nine bytes in length. By using longer NOP instructions, we can more
 * efficiently pad unused space with the processor being able to skip more
 * bytes per execution cycle.
 */
void
gum_x86_writer_put_nop_padding (GumX86Writer * self,
                                guint n)
{
  static const struct {
    guint8 one[1];
    guint8 two[2];
    guint8 three[3];
    guint8 four[4];
    guint8 five[5];
    guint8 six[6];
    guint8 seven[7];
    guint8 eight[8];
    guint8 nine[9];
  } nops = {
    /* NOP */
    .one =   { 0x90 },
    /* 66 NOP */
    .two =   { 0x66, 0x90 },
    /* NOP DWORD ptr [EAX] */
    .three = { 0x0f, 0x1f, 0x00 },
    /* NOP DWORD ptr [EAX + 00H] */
    .four =  { 0x0f, 0x1f, 0x40, 0x00 },
    /* NOP DWORD ptr [EAX + EAX*1 + 00H] */
    .five =  { 0x0f, 0x1f, 0x44, 0x00, 0x00 },
    /* 66 NOP DWORD ptr [EAX + EAX*1 + 00H] */
    .six =   { 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00 },
    /* NOP DWORD ptr [EAX + 00000000H] */
    .seven = { 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00 },
    /* NOP DWORD ptr [EAX + EAX*1 + 00000000H] */
    .eight = { 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /* 66 NOP DWORD ptr [EAX + EAX*1 + 00000000H] */
    .nine =  { 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
  };
  static const guint8 * nop_index[9] = {
    nops.one,
    nops.two,
    nops.three,
    nops.four,
    nops.five,
    nops.six,
    nops.seven,
    nops.eight,
    nops.nine,
  };
  static const guint max_nop = G_N_ELEMENTS (nop_index);
  guint remaining;

  for (remaining = n; remaining != 0; remaining -= max_nop)
  {
    if (remaining < max_nop)
    {
      gum_memcpy (self->code, nop_index[remaining - 1], remaining);
      gum_x86_writer_commit (self, remaining);
      break;
    }

    gum_memcpy (self->code, nop_index[max_nop - 1], max_nop);
    gum_x86_writer_commit (self, max_nop);
  }
}

gboolean
gum_x86_writer_put_fxsave_reg_ptr (GumX86Writer * self,
                                   GumX86Reg reg)
{
  return gum_x86_writer_put_fx_save_or_restore_reg_ptr (self, 0, reg);
}

gboolean
gum_x86_writer_put_fxrstor_reg_ptr (GumX86Writer * self,
                                    GumX86Reg reg)
{
  return gum_x86_writer_put_fx_save_or_restore_reg_ptr (self, 1, reg);
}

static gboolean
gum_x86_writer_put_fx_save_or_restore_reg_ptr (GumX86Writer * self,
                                               guint8 operation,
                                               GumX86Reg reg)
{
  GumX86RegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri.width != 32 || ri.index_is_extended)
      return FALSE;
  }
  else
  {
    if (ri.width != 64)
      return FALSE;
  }

  if (!gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL))
    return FALSE;

  self->code[0] = 0x0f;
  self->code[1] = 0xae;
  self->code[2] = (operation << 3) | ri.index;
  gum_x86_writer_commit (self, 3);

  if (ri.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  return TRUE;
}

gboolean
gum_x86_writer_put_vmovdqu64_reg_offset_ptr_zmm (GumX86Writer * self,
                                                 GumX86Reg dst_base,
                                                 gssize dst_offset,
                                                 guint src_zmm)
{
  return gum_x86_writer_put_vector_base_offset (self, 1, 2, TRUE, 2, 0x7f,
      src_zmm, 0, 64, dst_base, dst_offset, FALSE, 0);
}

gboolean
gum_x86_writer_put_vmovdqu64_zmm_reg_offset_ptr (GumX86Writer * self,
                                                 guint dst_zmm,
                                                 GumX86Reg src_base,
                                                 gssize src_offset)
{
  return gum_x86_writer_put_vector_base_offset (self, 1, 2, TRUE, 2, 0x6f,
      dst_zmm, 0, 64, src_base, src_offset, FALSE, 0);
}

gboolean
gum_x86_writer_put_vextracti64x4_reg_offset_ptr_zmm (GumX86Writer * self,
                                                     GumX86Reg dst_base,
                                                     gssize dst_offset,
                                                     guint src_zmm,
                                                     guint8 imm)
{
  return gum_x86_writer_put_vector_base_offset (self, 3, 1, TRUE, 2, 0x3b,
      src_zmm, 0, 32, dst_base, dst_offset, TRUE, imm);
}

gboolean
gum_x86_writer_put_vinserti64x4_zmm_reg_offset_ptr (GumX86Writer * self,
                                                    guint dst_zmm,
                                                    GumX86Reg src_base,
                                                    gssize src_offset,
                                                    guint8 imm)
{
  return gum_x86_writer_put_vector_base_offset (self, 3, 1, TRUE, 2, 0x3a,
      dst_zmm, dst_zmm, 32, src_base, src_offset, TRUE, imm);
}

gboolean
gum_x86_writer_put_kmovq_reg_offset_ptr_kreg (GumX86Writer * self,
                                              GumX86Reg dst_base,
                                              gssize dst_offset,
                                              guint src_kreg)
{
  return gum_x86_writer_put_kmov_base_offset (self, 0x91, src_kreg, dst_base,
      dst_offset);
}

gboolean
gum_x86_writer_put_kmovq_kreg_reg_offset_ptr (GumX86Writer * self,
                                              guint dst_kreg,
                                              GumX86Reg src_base,
                                              gssize src_offset)
{
  return gum_x86_writer_put_kmov_base_offset (self, 0x90, dst_kreg, src_base,
      src_offset);
}

static gboolean
gum_x86_writer_put_vector_base_offset (GumX86Writer * self,
                                       guint map,
                                       guint pp,
                                       gboolean w,
                                       guint length,
                                       guint8 opcode,
                                       guint reg,
                                       guint vvvv,
                                       guint tuple,
                                       GumX86Reg base_reg,
                                       gssize offset,
                                       gboolean has_imm,
                                       guint8 imm)
{
  GumX86RegInfo base;
  guint r, x, b, r_prime, vvvv_field, v_prime, mod;
  gboolean base_forces_disp, has_disp, disp_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, base_reg, &base);
  if (base.width != 64)
    return FALSE;

  base_forces_disp = base.meta == GUM_X86_META_XBP;
  has_disp = offset != 0 || base_forces_disp;
  disp_fits_in_i8 = has_disp && offset % (gssize) tuple == 0 &&
      GUM_IS_WITHIN_INT8_RANGE (offset / (gssize) tuple);
  mod = !has_disp ? 0 : (disp_fits_in_i8 ? 1 : 2);

  r = (reg & 8) ? 0 : 1;
  x = 1;
  b = base.index_is_extended ? 0 : 1;
  r_prime = (reg & 16) ? 0 : 1;
  vvvv_field = ~vvvv & 0xf;
  v_prime = (vvvv & 16) ? 0 : 1;

  gum_x86_writer_put_u8 (self, 0x62);
  gum_x86_writer_put_u8 (self,
      (r << 7) | (x << 6) | (b << 5) | (r_prime << 4) | (map & 0x3));
  gum_x86_writer_put_u8 (self,
      (w ? 0x80 : 0) | (vvvv_field << 3) | (1 << 2) | (pp & 0x3));
  gum_x86_writer_put_u8 (self, ((length & 0x3) << 5) | (v_prime << 3));
  gum_x86_writer_put_u8 (self, opcode);
  gum_x86_writer_put_u8 (self,
      (mod << 6) | ((reg & 0x7) << 3) | (base.index & 0x7));

  if (base.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  if (mod == 1)
    gum_x86_writer_put_s8 (self, (gint8) (offset / (gssize) tuple));
  else if (mod == 2)
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  if (has_imm)
    gum_x86_writer_put_u8 (self, imm);

  return TRUE;
}

static gboolean
gum_x86_writer_put_kmov_base_offset (GumX86Writer * self,
                                     guint8 opcode,
                                     guint mask_reg,
                                     GumX86Reg base_reg,
                                     gssize offset)
{
  GumX86RegInfo base;
  guint b, mod;
  gboolean base_forces_disp, has_disp, disp_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, base_reg, &base);
  if (base.width != 64)
    return FALSE;

  base_forces_disp = base.meta == GUM_X86_META_XBP;
  has_disp = offset != 0 || base_forces_disp;
  disp_fits_in_i8 = has_disp && GUM_IS_WITHIN_INT8_RANGE (offset);
  mod = !has_disp ? 0 : (disp_fits_in_i8 ? 1 : 2);

  b = base.index_is_extended ? 0 : 1;

  gum_x86_writer_put_u8 (self, 0xc4);
  gum_x86_writer_put_u8 (self, (1 << 7) | (1 << 6) | (b << 5) | 0x01);
  gum_x86_writer_put_u8 (self, (1 << 7) | (0xf << 3));
  gum_x86_writer_put_u8 (self, opcode);
  gum_x86_writer_put_u8 (self,
      (mod << 6) | ((mask_reg & 0x7) << 3) | (base.index & 0x7));

  if (base.index == 4)
    gum_x86_writer_put_u8 (self, 0x24);

  if (mod == 1)
    gum_x86_writer_put_s8 (self, (gint8) offset);
  else if (mod == 2)
  {
    *((gint32 *) self->code) = GINT32_TO_LE (offset);
    gum_x86_writer_commit (self, 4);
  }

  return TRUE;
}

void
gum_x86_writer_put_u8 (GumX86Writer * self,
                       guint8 value)
{
  *self->code = value;
  gum_x86_writer_commit (self, 1);
}

void
gum_x86_writer_put_s8 (GumX86Writer * self,
                       gint8 value)
{
  *((gint8 *) self->code) = value;
  gum_x86_writer_commit (self, 1);
}

void
gum_x86_writer_put_bytes (GumX86Writer * self,
                          const guint8 * data,
                          guint n)
{
  gum_memcpy (self->code, data, n);
  gum_x86_writer_commit (self, n);
}

static void
gum_x86_writer_describe_cpu_reg (GumX86Writer * self,
                                 GumX86Reg reg,
                                 GumX86RegInfo * ri)
{
  if (reg >= GUM_X86_XAX && reg <= GUM_X86_XDI)
  {
    if (self->target_cpu == GUM_CPU_IA32)
      reg = (GumX86Reg) (GUM_X86_EAX + reg - GUM_X86_XAX);
    else
      reg = (GumX86Reg) (GUM_X86_RAX + reg - GUM_X86_XAX);
  }

  ri->meta = gum_meta_reg_from_cpu_reg (reg);

  if (reg >= GUM_X86_RAX && reg <= GUM_X86_R15)
  {
    ri->width = 64;

    if (reg < GUM_X86_R8)
    {
      ri->index = reg - GUM_X86_RAX;
      ri->index_is_extended = FALSE;
    }
    else
    {
      ri->index = reg - GUM_X86_R8;
      ri->index_is_extended = TRUE;
    }
  }
  else
  {
    ri->width = 32;

    if (reg < GUM_X86_R8D)
    {
      ri->index = reg - GUM_X86_EAX;
      ri->index_is_extended = FALSE;
    }
    else
    {
      ri->index = reg - GUM_X86_R8D;
      ri->index_is_extended = TRUE;
    }
  }
}

static GumX86MetaReg
gum_meta_reg_from_cpu_reg (GumX86Reg reg)
{
  if (reg >= GUM_X86_EAX && reg <= GUM_X86_R15D)
    return (GumX86MetaReg) (GUM_X86_META_XAX + reg - GUM_X86_EAX);

  if (reg >= GUM_X86_RAX && reg <= GUM_X86_R15)
    return (GumX86MetaReg) (GUM_X86_META_XAX + reg - GUM_X86_RAX);

  return (GumX86MetaReg) (GUM_X86_META_XAX + reg - GUM_X86_XAX);
}

static gboolean
gum_x86_writer_put_prefix_for_reg_info (GumX86Writer * self,
                                        const GumX86RegInfo * ri,
                                        guint operand_index)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ri->width != 32 || ri->index_is_extended)
      return FALSE;
  }
  else
  {
    guint mask;

    mask = 1 << (operand_index * 2);

    if (ri->width == 32)
    {
      if (ri->index_is_extended)
        gum_x86_writer_put_u8 (self, 0x40 | mask);
    }
    else
    {
      gum_x86_writer_put_u8 (self,
          (ri->index_is_extended) ? 0x48 | mask : 0x48);
    }
  }

  return TRUE;
}

/* TODO: improve this function and get rid of the one above */
static gboolean
gum_x86_writer_put_prefix_for_registers (GumX86Writer * self,
                                         const GumX86RegInfo * width_reg,
                                         guint default_width,
                                         ...)
{
  const GumX86RegInfo * ra, * rb, * rc;
  va_list args;

  va_start (args, default_width);

  ra = va_arg (args, const GumX86RegInfo *);
  g_assert (ra != NULL);

  rb = va_arg (args, const GumX86RegInfo *);
  if (rb != NULL)
  {
    rc = va_arg (args, const GumX86RegInfo *);
  }
  else
  {
    rc = NULL;
  }

  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (ra->width != 32 || ra->index_is_extended)
      return FALSE;
    if (rb != NULL && (rb->width != 32 || rb->index_is_extended))
      return FALSE;
    if (rc != NULL && (rc->width != 32 || rc->index_is_extended))
      return FALSE;
  }
  else
  {
    guint nibble = 0;

    if (width_reg->width != default_width)
      nibble |= 0x8;
    if (rb != NULL && rb->index_is_extended)
      nibble |= 0x4;
    if (rc != NULL && rc->index_is_extended)
      nibble |= 0x2;
    if (ra->index_is_extended)
      nibble |= 0x1;

    if (nibble != 0)
      gum_x86_writer_put_u8 (self, 0x40 | nibble);
  }

  return TRUE;
}

static guint8
gum_get_jcc_opcode (x86_insn instruction_id)
{
  switch (instruction_id)
  {
    case X86_INS_JO:
      return 0x70;
    case X86_INS_JNO:
      return 0x71;
    case X86_INS_JB:
      return 0x72;
    case X86_INS_JAE:
      return 0x73;
    case X86_INS_JE:
      return 0x74;
    case X86_INS_JNE:
      return 0x75;
    case X86_INS_JBE:
      return 0x76;
    case X86_INS_JA:
      return 0x77;
    case X86_INS_JS:
      return 0x78;
    case X86_INS_JNS:
      return 0x79;
    case X86_INS_JP:
      return 0x7a;
    case X86_INS_JNP:
      return 0x7b;
    case X86_INS_JL:
      return 0x7c;
    case X86_INS_JGE:
      return 0x7d;
    case X86_INS_JLE:
      return 0x7e;
    case X86_INS_JG:
      return 0x7f;
    case X86_INS_JCXZ:
    case X86_INS_JECXZ:
    case X86_INS_JRCXZ:
    default:
      return 0xe3;
  }
}
