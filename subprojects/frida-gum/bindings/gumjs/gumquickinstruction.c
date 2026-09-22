/*
 * Copyright (C) 2020-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 EvilWind <evilwind@protonmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickinstruction.h"

#include "gumquickmacros.h"

#include <string.h>

GUMJS_DECLARE_FUNCTION (gumjs_instruction_parse)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_instruction_construct)
GUMJS_DECLARE_FINALIZER (gumjs_instruction_finalize)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_address)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_next)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_size)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_mnemonic)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_op_str)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_operands)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_regs_accessed)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_regs_read)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_regs_written)
GUMJS_DECLARE_GETTER (gumjs_instruction_get_groups)
GUMJS_DECLARE_FUNCTION (gumjs_instruction_to_string)
GUMJS_DECLARE_FUNCTION (gumjs_instruction_to_json)

static JSValue gum_parse_operands (JSContext * ctx, const cs_insn * insn,
    csh cs, GumQuickCore * core);

#if defined (HAVE_I386)
static JSValue gum_x86_parse_memory_operand_value (JSContext * ctx,
    const x86_op_mem * mem, csh cs, GumQuickCore * core);
#elif defined (HAVE_ARM)
static JSValue gum_arm_parse_memory_operand_value (JSContext * ctx,
    const arm_op_mem * mem, csh cs, GumQuickCore * core);
static JSValue gum_arm_parse_shift_details (JSContext * ctx,
    const cs_arm_op * op, GumQuickCore * core);
static const gchar * gum_arm_shifter_to_string (arm_shifter type);
#elif defined (HAVE_ARM64)
static JSValue gum_arm64_parse_memory_operand_value (JSContext * ctx,
    const arm64_op_mem * mem, csh cs, GumQuickCore * core);
static JSValue gum_arm64_parse_shift_details (JSContext * ctx,
    const cs_arm64_op * op, GumQuickCore * core);
static const gchar * gum_arm64_shifter_to_string (arm64_shifter type);
static const gchar * gum_arm64_extender_to_string (arm64_extender ext);
static const gchar * gum_arm64_vas_to_string (arm64_vas vas);
#elif defined (HAVE_MIPS)
static JSValue gum_mips_parse_memory_operand_value (JSContext * ctx,
    const mips_op_mem * mem, csh cs, GumQuickCore * core);
#endif

static JSValue gum_parse_regs (JSContext * ctx, const uint16_t * regs,
    uint8_t count, csh cs);

static JSValue gum_parse_groups (JSContext * ctx, const uint8_t * groups,
    uint8_t count, csh cs);

G_GNUC_UNUSED static JSValue gum_access_type_to_string (JSContext * ctx,
    uint8_t access_type);

static const JSClassDef gumjs_instruction_def =
{
  .class_name = "Instruction",
  .finalizer = gumjs_instruction_finalize,
};

static const JSCFunctionListEntry gumjs_instruction_module_entries[] =
{
  JS_CFUNC_DEF ("_parse", 0, gumjs_instruction_parse),
};

static const JSCFunctionListEntry gumjs_instruction_entries[] =
{
  JS_CGETSET_DEF ("address", gumjs_instruction_get_address, NULL),
  JS_CGETSET_DEF ("next", gumjs_instruction_get_next, NULL),
  JS_CGETSET_DEF ("size", gumjs_instruction_get_size, NULL),
  JS_CGETSET_DEF ("mnemonic", gumjs_instruction_get_mnemonic, NULL),
  JS_CGETSET_DEF ("opStr", gumjs_instruction_get_op_str, NULL),
  JS_CGETSET_DEF ("operands", gumjs_instruction_get_operands, NULL),
  JS_CGETSET_DEF ("regsAccessed", gumjs_instruction_get_regs_accessed, NULL),
  JS_CGETSET_DEF ("regsRead", gumjs_instruction_get_regs_read, NULL),
  JS_CGETSET_DEF ("regsWritten", gumjs_instruction_get_regs_written, NULL),
  JS_CGETSET_DEF ("groups", gumjs_instruction_get_groups, NULL),
  JS_CFUNC_DEF ("toString", 0, gumjs_instruction_to_string),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_instruction_to_json),
};

void
_gum_quick_instruction_init (GumQuickInstruction * self,
                             JSValue ns,
                             GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  gum_cs_arch_register_native ();
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &self->capstone);
  cs_option (self->capstone, CS_OPT_DETAIL, CS_OPT_ON);

  _gum_quick_core_store_module_data (core, "instruction", self);

  _gum_quick_create_class (ctx, &gumjs_instruction_def, core,
      &self->instruction_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_instruction_construct,
      gumjs_instruction_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, ctor, gumjs_instruction_module_entries,
      G_N_ELEMENTS (gumjs_instruction_module_entries));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_instruction_entries,
      G_N_ELEMENTS (gumjs_instruction_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_instruction_def.class_name, ctor,
      JS_PROP_C_W_E);
}

void
_gum_quick_instruction_dispose (GumQuickInstruction * self)
{
}

void
_gum_quick_instruction_finalize (GumQuickInstruction * self)
{
  cs_close (&self->capstone);
}

static GumQuickInstruction *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "instruction");
}

JSValue
_gum_quick_instruction_new (JSContext * ctx,
                            const cs_insn * insn,
                            gboolean is_owned,
                            gconstpointer target,
                            csh capstone,
                            GumQuickInstruction * parent,
                            GumQuickInstructionValue ** instruction)
{
  JSValue wrapper;
  GumQuickInstructionValue * v;

  wrapper = JS_NewObjectClass (ctx, parent->instruction_class);

  v = g_slice_new (GumQuickInstructionValue);
  v->wrapper = wrapper;
  if (is_owned)
  {
    v->insn = insn;
  }
  else
  {
    cs_insn * insn_copy;
    cs_detail * detail_copy;

    g_assert (capstone != 0);

    insn_copy = cs_malloc (capstone);
    detail_copy = insn_copy->detail;
    memcpy (insn_copy, insn, sizeof (cs_insn));
    insn_copy->detail = detail_copy;
    if (detail_copy != NULL)
      memcpy (detail_copy, insn->detail, sizeof (cs_detail));

    v->insn = insn_copy;
  }
  v->owns_memory = insn != NULL;
  v->target = target;

  JS_SetOpaque (wrapper, v);

  if (instruction != NULL)
    *instruction = v;

  return wrapper;
}

gboolean
_gum_quick_instruction_get (JSContext * ctx,
                            JSValue val,
                            GumQuickInstruction * parent,
                            GumQuickInstructionValue ** instruction)
{
  GumQuickInstructionValue * v;

  if (!_gum_quick_unwrap (ctx, val, parent->instruction_class, parent->core,
      (gpointer *) &v))
    return FALSE;

  if (v->insn == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *instruction = v;
  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_instruction_parse)
{
  GumQuickInstruction * self;
  gpointer target;
  uint64_t address;
  const gsize max_instruction_size = 16;
  cs_insn * insn;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "p", &target))
    return JS_EXCEPTION;

  target = gum_strip_code_pointer (target);

#ifdef HAVE_ARM
  address = GPOINTER_TO_SIZE (target) & ~1;
  cs_option (self->capstone, CS_OPT_MODE,
      (((GPOINTER_TO_SIZE (target) & 1) == 1) ? CS_MODE_THUMB : CS_MODE_ARM) |
      CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN);
#else
  address = GPOINTER_TO_SIZE (target);
#endif

  gum_ensure_code_readable (GSIZE_TO_POINTER (address), max_instruction_size);

  if (cs_disasm (self->capstone, (uint8_t *) GSIZE_TO_POINTER (address),
      max_instruction_size, address, 1, &insn) == 0)
  {
    return _gum_quick_throw_literal (ctx, "invalid instruction");
  }

  return _gum_quick_instruction_new (ctx, insn, TRUE, target, self->capstone,
      self, NULL);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_instruction_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FINALIZER (gumjs_instruction_finalize)
{
  GumQuickInstructionValue * v;

  v = JS_GetOpaque (val, gumjs_get_parent_module (core)->instruction_class);
  if (v == NULL)
    return;

  if (v->owns_memory && v->insn != NULL)
    cs_free ((cs_insn *) v->insn, 1);

  g_slice_free (GumQuickInstructionValue, v);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_address)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx,
      GSIZE_TO_POINTER (self->insn->address), core);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_next)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx,
      GSIZE_TO_POINTER (GPOINTER_TO_SIZE (self->target) + self->insn->size),
      core);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_size)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewInt32 (ctx, self->insn->size);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_mnemonic)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewString (ctx, self->insn->mnemonic);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_op_str)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewString (ctx, self->insn->op_str);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_operands)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return gum_parse_operands (ctx, self->insn, parent->capstone, parent->core);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_regs_accessed)
{
  JSValue result;
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;
  csh capstone;
  cs_regs regs_read, regs_write;
  uint8_t regs_read_count, regs_write_count;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  capstone = parent->capstone;

  if (cs_regs_access (capstone, self->insn,
        regs_read, &regs_read_count,
        regs_write, &regs_write_count) != 0)
  {
    return _gum_quick_throw_literal (ctx,
        "not yet supported on this architecture");
  }

  result = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, result,
      GUM_QUICK_CORE_ATOM (core, read),
      gum_parse_regs (ctx, regs_read, regs_read_count, capstone),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, result,
      GUM_QUICK_CORE_ATOM (core, written),
      gum_parse_regs (ctx, regs_write, regs_write_count, capstone),
      JS_PROP_C_W_E);

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_regs_read)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;
  const cs_detail * d;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  d = self->insn->detail;

  return gum_parse_regs (ctx, d->regs_read, d->regs_read_count,
      parent->capstone);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_regs_written)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;
  const cs_detail * d;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  d = self->insn->detail;

  return gum_parse_regs (ctx, d->regs_write, d->regs_write_count,
      parent->capstone);
}

GUMJS_DEFINE_GETTER (gumjs_instruction_get_groups)
{
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;
  const cs_detail * d;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  d = self->insn->detail;

  return gum_parse_groups (ctx, d->groups, d->groups_count, parent->capstone);
}

GUMJS_DEFINE_FUNCTION (gumjs_instruction_to_string)
{
  JSValue result;
  GumQuickInstruction * parent;
  GumQuickInstructionValue * self;
  const cs_insn * insn;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_instruction_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  insn = self->insn;

  if (insn->op_str[0] == '\0')
  {
    result = JS_NewString (ctx, insn->mnemonic);
  }
  else
  {
    gchar * str;

    str = g_strconcat (insn->mnemonic, " ", insn->op_str, NULL);
    result = JS_NewString (ctx, str);
    g_free (str);
  }

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_instruction_to_json)
{
  JSValue result;
  guint i;

  result = JS_NewObject (ctx);

  for (i = 0; i != G_N_ELEMENTS (gumjs_instruction_entries); i++)
  {
    const JSCFunctionListEntry * e = &gumjs_instruction_entries[i];
    JSValue val;

    if (e->def_type != JS_DEF_CGETSET)
      continue;

    val = JS_GetPropertyStr (ctx, this_val, e->name);
    if (JS_IsException (val))
      goto propagate_exception;
    JS_SetPropertyStr (ctx, result, e->name, val);
  }

  return result;

propagate_exception:
  {
    JS_FreeValue (ctx, result);

    return JS_EXCEPTION;
  }
}

#if defined (HAVE_I386)

static JSValue
gum_parse_operands (JSContext * ctx,
                    const cs_insn * insn,
                    csh cs,
                    GumQuickCore * core)
{
  JSValue result;
  const cs_x86 * x86 = &insn->detail->x86;
  uint8_t i;

  result = JS_NewArray (ctx);

  for (i = 0; i != x86->op_count; i++)
  {
    const cs_x86_op * op = &x86->operands[i];
    JSValue op_obj;
    const gchar * type;
    JSValue val;

    op_obj = JS_NewObject (ctx);

    switch (op->type)
    {
      case X86_OP_REG:
        type = "reg";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case X86_OP_IMM:
        type = "imm";
        val = _gum_quick_int64_new (ctx, op->imm, core);
        break;
      case X86_OP_MEM:
        type = "mem";
        val = gum_x86_parse_memory_operand_value (ctx, &op->mem, cs, core);
        break;
      default:
        type = NULL;
        val = JS_NULL;
        g_assert_not_reached ();
    }

    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, type),
        JS_NewString (ctx, type),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, value),
        val,
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, size),
        JS_NewInt32 (ctx, op->size),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, access),
        gum_access_type_to_string (ctx, op->access),
        JS_PROP_C_W_E);

    JS_DefinePropertyValueUint32 (ctx, result, i, op_obj, JS_PROP_C_W_E);
  }

  return result;
}

static JSValue
gum_x86_parse_memory_operand_value (JSContext * ctx,
                                    const x86_op_mem * mem,
                                    csh cs,
                                    GumQuickCore * core)
{
  JSValue val = JS_NewObject (ctx);

  if (mem->segment != X86_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, segment),
        JS_NewString (ctx, cs_reg_name (cs, mem->segment)),
        JS_PROP_C_W_E);
  }
  if (mem->base != X86_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, base),
        JS_NewString (ctx, cs_reg_name (cs, mem->base)),
        JS_PROP_C_W_E);
  }
  if (mem->index != X86_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, index),
        JS_NewString (ctx, cs_reg_name (cs, mem->index)),
        JS_PROP_C_W_E);
  }
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, scale),
      JS_NewInt32 (ctx, mem->scale),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, disp),
      JS_NewInt64 (ctx, mem->disp),
      JS_PROP_C_W_E);

  return val;
}

#elif defined (HAVE_ARM)

static JSValue
gum_parse_operands (JSContext * ctx,
                    const cs_insn * insn,
                    csh cs,
                    GumQuickCore * core)
{
  JSValue result;
  const cs_arm * arm = &insn->detail->arm;
  uint8_t i;

  result = JS_NewArray (ctx);

  for (i = 0; i != arm->op_count; i++)
  {
    const cs_arm_op * op = &arm->operands[i];
    JSValue op_obj;
    const gchar * type;
    JSValue val;

    op_obj = JS_NewObject (ctx);

    switch (op->type)
    {
      case ARM_OP_REG:
        type = "reg";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case ARM_OP_IMM:
        type = "imm";
        val = JS_NewInt32 (ctx, op->imm);
        break;
      case ARM_OP_MEM:
        type = "mem";
        val = gum_arm_parse_memory_operand_value (ctx, &op->mem, cs, core);
        break;
      case ARM_OP_FP:
        type = "fp";
        val = JS_NewFloat64 (ctx, op->fp);
        break;
      case ARM_OP_CIMM:
        type = "cimm";
        val = JS_NewInt32 (ctx, op->imm);
        break;
      case ARM_OP_PIMM:
        type = "pimm";
        val = JS_NewInt32 (ctx, op->imm);
        break;
      case ARM_OP_SETEND:
        type = "setend";
        val = JS_NewString (ctx, (op->setend == ARM_SETEND_BE) ? "be" : "le");
        break;
      case ARM_OP_SYSREG:
        type = "sysreg";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      default:
        g_assert_not_reached ();
    }

    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, type),
        JS_NewString (ctx, type),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, value),
        val,
        JS_PROP_C_W_E);
    if (op->shift.type != ARM_SFT_INVALID)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, shift),
          gum_arm_parse_shift_details (ctx, op, core),
          JS_PROP_C_W_E);
    }
    if (op->vector_index != -1)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, vectorIndex),
          JS_NewInt32 (ctx, op->vector_index),
          JS_PROP_C_W_E);
    }
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, subtracted),
        JS_NewBool (ctx, op->subtracted),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, access),
        gum_access_type_to_string (ctx, op->access),
        JS_PROP_C_W_E);

    JS_DefinePropertyValueUint32 (ctx, result, i, op_obj, JS_PROP_C_W_E);
  }

  return result;
}

static JSValue
gum_arm_parse_memory_operand_value (JSContext * ctx,
                                    const arm_op_mem * mem,
                                    csh cs,
                                    GumQuickCore * core)
{
  JSValue val = JS_NewObject (ctx);

  if (mem->base != ARM_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, base),
        JS_NewString (ctx, cs_reg_name (cs, mem->base)),
        JS_PROP_C_W_E);
  }
  if (mem->index != ARM_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, index),
        JS_NewString (ctx, cs_reg_name (cs, mem->index)),
        JS_PROP_C_W_E);
  }
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, scale),
      JS_NewInt32 (ctx, mem->scale),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, disp),
      JS_NewInt32 (ctx, mem->disp),
      JS_PROP_C_W_E);

  return val;
}

static JSValue
gum_arm_parse_shift_details (JSContext * ctx,
                             const cs_arm_op * op,
                             GumQuickCore * core)
{
  JSValue shift = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, shift,
      GUM_QUICK_CORE_ATOM (core, type),
      JS_NewString (ctx, gum_arm_shifter_to_string (op->shift.type)),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, shift,
      GUM_QUICK_CORE_ATOM (core, value),
      JS_NewInt64 (ctx, op->shift.value),
      JS_PROP_C_W_E);

  return shift;
}

static const gchar *
gum_arm_shifter_to_string (arm_shifter type)
{
  switch (type)
  {
    case ARM_SFT_ASR: return "asr";
    case ARM_SFT_LSL: return "lsl";
    case ARM_SFT_LSR: return "lsr";
    case ARM_SFT_ROR: return "ror";
    case ARM_SFT_RRX: return "rrx";
    case ARM_SFT_ASR_REG: return "asr-reg";
    case ARM_SFT_LSL_REG: return "lsl-reg";
    case ARM_SFT_LSR_REG: return "lsr-reg";
    case ARM_SFT_ROR_REG: return "ror-reg";
    case ARM_SFT_RRX_REG: return "rrx-reg";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}

#elif defined (HAVE_ARM64)

static JSValue
gum_parse_operands (JSContext * ctx,
                    const cs_insn * insn,
                    csh cs,
                    GumQuickCore * core)
{
  JSValue result;
  const cs_arm64 * arm64 = &insn->detail->arm64;
  uint8_t i;

  result = JS_NewArray (ctx);

  for (i = 0; i != arm64->op_count; i++)
  {
    const cs_arm64_op * op = &arm64->operands[i];
    JSValue op_obj;
    const gchar * type;
    JSValue val;

    op_obj = JS_NewObject (ctx);

    switch (op->type)
    {
      case ARM64_OP_REG:
        type = "reg";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case ARM64_OP_IMM:
        type = "imm";
        val = _gum_quick_int64_new (ctx, op->imm, core);
        break;
      case ARM64_OP_MEM:
        type = "mem";
        val = gum_arm64_parse_memory_operand_value (ctx, &op->mem, cs, core);
        break;
      case ARM64_OP_FP:
        type = "fp";
        val = JS_NewFloat64 (ctx, op->fp);
        break;
      case ARM64_OP_CIMM:
        type = "cimm";
        val = _gum_quick_int64_new (ctx, op->imm, core);
        break;
      case ARM64_OP_REG_MRS:
        type = "reg-mrs";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case ARM64_OP_REG_MSR:
        type = "reg-msr";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case ARM64_OP_PSTATE:
        type = "pstate";
        val = JS_NewInt32 (ctx, op->pstate);
        break;
      case ARM64_OP_SYS:
        type = "sys";
        val = JS_NewInt64 (ctx, op->sys);
        break;
      case ARM64_OP_PREFETCH:
        type = "prefetch";
        val = JS_NewInt32 (ctx, op->prefetch);
        break;
      case ARM64_OP_BARRIER:
        type = "barrier";
        val = JS_NewInt32 (ctx, op->barrier);
        break;
      default:
        g_assert_not_reached ();
    }

    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, type),
        JS_NewString (ctx, type),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, value),
        val,
        JS_PROP_C_W_E);
    if (op->shift.type != ARM64_SFT_INVALID)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, shift),
          gum_arm64_parse_shift_details (ctx, op, core),
          JS_PROP_C_W_E);
    }
    if (op->ext != ARM64_EXT_INVALID)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, ext),
          JS_NewString (ctx, gum_arm64_extender_to_string (op->ext)),
          JS_PROP_C_W_E);
    }
    if (op->vas != ARM64_VAS_INVALID)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, vas),
          JS_NewString (ctx, gum_arm64_vas_to_string (op->vas)),
          JS_PROP_C_W_E);
    }
    if (op->vector_index != -1)
    {
      JS_DefinePropertyValue (ctx, op_obj,
          GUM_QUICK_CORE_ATOM (core, vectorIndex),
          JS_NewInt32 (ctx, op->vector_index),
          JS_PROP_C_W_E);
    }
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, access),
        gum_access_type_to_string (ctx, op->access),
        JS_PROP_C_W_E);

    JS_DefinePropertyValueUint32 (ctx, result, i, op_obj, JS_PROP_C_W_E);
  }

  return result;
}

static JSValue
gum_arm64_parse_memory_operand_value (JSContext * ctx,
                                      const arm64_op_mem * mem,
                                      csh cs,
                                      GumQuickCore * core)
{
  JSValue val = JS_NewObject (ctx);

  if (mem->base != ARM64_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, base),
        JS_NewString (ctx, cs_reg_name (cs, mem->base)),
        JS_PROP_C_W_E);
  }
  if (mem->index != ARM64_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, index),
        JS_NewString (ctx, cs_reg_name (cs, mem->index)),
        JS_PROP_C_W_E);
  }
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, disp),
      JS_NewInt32 (ctx, mem->disp),
      JS_PROP_C_W_E);

  return val;
}

static JSValue
gum_arm64_parse_shift_details (JSContext * ctx,
                               const cs_arm64_op * op,
                               GumQuickCore * core)
{
  JSValue shift = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, shift,
      GUM_QUICK_CORE_ATOM (core, type),
      JS_NewString (ctx, gum_arm64_shifter_to_string (op->shift.type)),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, shift,
      GUM_QUICK_CORE_ATOM (core, value),
      JS_NewInt64 (ctx, op->shift.value),
      JS_PROP_C_W_E);

  return shift;
}

static const gchar *
gum_arm64_shifter_to_string (arm64_shifter type)
{
  switch (type)
  {
    case ARM64_SFT_LSL: return "lsl";
    case ARM64_SFT_MSL: return "msl";
    case ARM64_SFT_LSR: return "lsr";
    case ARM64_SFT_ASR: return "asr";
    case ARM64_SFT_ROR: return "ror";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}

static const gchar *
gum_arm64_extender_to_string (arm64_extender ext)
{
  switch (ext)
  {
    case ARM64_EXT_UXTB: return "uxtb";
    case ARM64_EXT_UXTH: return "uxth";
    case ARM64_EXT_UXTW: return "uxtw";
    case ARM64_EXT_UXTX: return "uxtx";
    case ARM64_EXT_SXTB: return "sxtb";
    case ARM64_EXT_SXTH: return "sxth";
    case ARM64_EXT_SXTW: return "sxtw";
    case ARM64_EXT_SXTX: return "sxtx";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}

static const gchar *
gum_arm64_vas_to_string (arm64_vas vas)
{
  switch (vas)
  {
    case ARM64_VAS_8B:  return "8b";
    case ARM64_VAS_16B: return "16b";
    case ARM64_VAS_4H:  return "4h";
    case ARM64_VAS_8H:  return "8h";
    case ARM64_VAS_2S:  return "2s";
    case ARM64_VAS_4S:  return "4s";
    case ARM64_VAS_1D:  return "1d";
    case ARM64_VAS_2D:  return "2d";
    case ARM64_VAS_1Q:  return "1q";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}

#elif defined (HAVE_MIPS)

static JSValue
gum_parse_operands (JSContext * ctx,
                    const cs_insn * insn,
                    csh cs,
                    GumQuickCore * core)
{
  JSValue result;
  const cs_mips * mips = &insn->detail->mips;
  uint8_t i;

  result = JS_NewArray (ctx);

  for (i = 0; i != mips->op_count; i++)
  {
    const cs_mips_op * op = &mips->operands[i];
    JSValue op_obj;
    const gchar * type;
    JSValue val;

    op_obj = JS_NewObject (ctx);

    switch (op->type)
    {
      case MIPS_OP_REG:
        type = "reg";
        val = JS_NewString (ctx, cs_reg_name (cs, op->reg));
        break;
      case MIPS_OP_IMM:
        type = "imm";
        val = JS_NewInt64 (ctx, op->imm);
        break;
      case MIPS_OP_MEM:
        type = "mem";
        val = gum_mips_parse_memory_operand_value (ctx, &op->mem, cs, core);
        break;
      default:
        g_assert_not_reached ();
    }

    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, type),
        JS_NewString (ctx, type),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op_obj,
        GUM_QUICK_CORE_ATOM (core, value),
        val,
        JS_PROP_C_W_E);

    JS_DefinePropertyValueUint32 (ctx, result, i, op_obj, JS_PROP_C_W_E);
  }

  return result;
}

static JSValue
gum_mips_parse_memory_operand_value (JSContext * ctx,
                                     const mips_op_mem * mem,
                                     csh cs,
                                     GumQuickCore * core)
{
  JSValue val = JS_NewObject (ctx);

  if (mem->base != MIPS_REG_INVALID)
  {
    JS_DefinePropertyValue (ctx, val,
        GUM_QUICK_CORE_ATOM (core, base),
        JS_NewString (ctx, cs_reg_name (cs, mem->base)),
        JS_PROP_C_W_E);
  }
  JS_DefinePropertyValue (ctx, val,
      GUM_QUICK_CORE_ATOM (core, disp),
      JS_NewInt64 (ctx, mem->disp),
      JS_PROP_C_W_E);

  return val;
}

#endif

static JSValue
gum_parse_regs (JSContext * ctx,
                const uint16_t * regs,
                uint8_t count,
                csh cs)
{
  JSValue r;
  uint8_t i;

  r = JS_NewArray (ctx);

  for (i = 0; i != count; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, r, i,
        JS_NewString (ctx, cs_reg_name (cs, regs[i])),
        JS_PROP_C_W_E);
  }

  return r;
}

static JSValue
gum_parse_groups (JSContext * ctx,
                  const uint8_t * groups,
                  uint8_t count,
                  csh cs)
{
  JSValue g;
  uint8_t i;

  g = JS_NewArray (ctx);

  for (i = 0; i != count; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, g, i,
        JS_NewString (ctx, cs_group_name (cs, groups[i])),
        JS_PROP_C_W_E);
  }

  return g;
}

static JSValue
gum_access_type_to_string (JSContext * ctx,
                           uint8_t access_type)
{
  const gchar * str = NULL;

  switch (access_type)
  {
    case CS_AC_INVALID:
      str = "";
      break;
    case CS_AC_READ:
      str = "r";
      break;
    case CS_AC_WRITE:
      str = "w";
      break;
    case CS_AC_READ | CS_AC_WRITE:
      str = "rw";
      break;
    default:
      g_assert_not_reached ();
  }

  return JS_NewString (ctx, str);
}
