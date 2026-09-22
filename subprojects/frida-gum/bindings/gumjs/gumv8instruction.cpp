/*
 * Copyright (C) 2014-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 EvilWind <evilwind@protonmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8instruction.h"

#include "gumv8macros.h"

#include <string.h>

#define GUMJS_MODULE_NAME Instruction

#define GUM_INSTRUCTION_FOOTPRINT_ESTIMATE 256

using namespace v8;

GUMJS_DECLARE_FUNCTION (gumjs_instruction_parse)

static GumV8InstructionValue * gum_v8_instruction_alloc (
    GumV8Instruction * module);
static void gum_v8_instruction_dispose (GumV8InstructionValue * self);
static void gum_v8_instruction_free (GumV8InstructionValue * self);
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
static void gum_v8_instruction_on_weak_notify (
    const WeakCallbackInfo<GumV8InstructionValue> & info);

static Local<Array> gum_parse_operands (const cs_insn * insn,
    GumV8Instruction * module);

#if defined (HAVE_I386)
static Local<Object> gum_x86_parse_memory_operand_value (const x86_op_mem * mem,
    GumV8Instruction * module);
#elif defined (HAVE_ARM)
static Local<Object> gum_arm_parse_memory_operand_value (const arm_op_mem * mem,
    GumV8Instruction * module);
static Local<Object> gum_arm_parse_shift_details (const cs_arm_op * op,
    GumV8Instruction * module);
static const gchar * gum_arm_shifter_to_string (arm_shifter type);
#elif defined (HAVE_ARM64)
static Local<Object> gum_arm64_parse_memory_operand_value (
    const arm64_op_mem * mem, GumV8Instruction * module);
static Local<Object> gum_arm64_parse_shift_details (const cs_arm64_op * op,
    GumV8Instruction * module);
static const gchar * gum_arm64_shifter_to_string (arm64_shifter type);
static const gchar * gum_arm64_extender_to_string (arm64_extender ext);
static const gchar * gum_arm64_vas_to_string (arm64_vas vas);
#elif defined (HAVE_MIPS)
static Local<Object> gum_mips_parse_memory_operand_value (
    const mips_op_mem * mem, GumV8Instruction * module);
#endif

static Local<Array> gum_parse_regs (const uint16_t * regs, uint8_t count,
    GumV8Instruction * module);

static Local<Array> gum_parse_groups (const uint8_t * groups, uint8_t count,
    GumV8Instruction * module);

static const gchar * gum_access_type_to_string (uint8_t access_type);

static const GumV8Function gumjs_instruction_module_functions[] =
{
  { "_parse", gumjs_instruction_parse },

  { NULL, NULL }
};

static const GumV8Property gumjs_instruction_values[] =
{
  { "address", gumjs_instruction_get_address, NULL },
  { "next", gumjs_instruction_get_next, NULL },
  { "size", gumjs_instruction_get_size, NULL },
  { "mnemonic", gumjs_instruction_get_mnemonic, NULL },
  { "opStr", gumjs_instruction_get_op_str, NULL },
  { "operands", gumjs_instruction_get_operands, NULL },
  { "regsAccessed", gumjs_instruction_get_regs_accessed, NULL },
  { "regsRead", gumjs_instruction_get_regs_read, NULL },
  { "regsWritten", gumjs_instruction_get_regs_written, NULL },
  { "groups", gumjs_instruction_get_groups, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_instruction_functions[] =
{
  { "toString", gumjs_instruction_to_string },

  { NULL, NULL }
};

void
_gum_v8_instruction_init (GumV8Instruction * self,
                          GumV8Core * core,
                          Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  gum_cs_arch_register_native ();
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &self->capstone);
  cs_option (self->capstone, CS_OPT_DETAIL, CS_OPT_ON);

  auto module = External::New (isolate, self);

  auto klass = _gum_v8_create_class ("Instruction", nullptr, scope, module,
      isolate);
  _gum_v8_class_add_static (klass, gumjs_instruction_module_functions, module,
      isolate);
  _gum_v8_class_add (klass, gumjs_instruction_values, module, isolate);
  _gum_v8_class_add (klass, gumjs_instruction_functions, module, isolate);
  self->klass = new Global<FunctionTemplate> (isolate, klass);
}

void
_gum_v8_instruction_realize (GumV8Instruction * self)
{
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  self->instructions = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_instruction_free);

  auto klass = Local<FunctionTemplate>::New (isolate, *self->klass);
  auto object = klass->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();
  self->template_object = new Global<Object> (isolate, object);
}

void
_gum_v8_instruction_dispose (GumV8Instruction * self)
{
  g_hash_table_unref (self->instructions);
  self->instructions = NULL;

  delete self->template_object;
  self->template_object = nullptr;

  delete self->klass;
  self->klass = nullptr;
}

void
_gum_v8_instruction_finalize (GumV8Instruction * self)
{
  cs_close (&self->capstone);
}

Local<Object>
_gum_v8_instruction_new (csh capstone,
                         const cs_insn * insn,
                         gboolean is_owned,
                         gconstpointer target,
                         GumV8Instruction * module)
{
  auto value = _gum_v8_instruction_new_persistent (module);

  if (is_owned)
  {
    value->insn = insn;
  }
  else
  {
    g_assert (capstone != 0);

    cs_insn * insn_copy = cs_malloc (capstone);
    cs_detail * detail_copy = insn_copy->detail;
    memcpy (insn_copy, insn, sizeof (cs_insn));
    insn_copy->detail = detail_copy;
    if (detail_copy != NULL)
      memcpy (detail_copy, insn->detail, sizeof (cs_detail));

    value->insn = insn_copy;
  }
  value->owns_memory = TRUE;
  value->target = target;

  value->object->SetWeak (value, gum_v8_instruction_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (module->instructions, value);

  return Local<Object>::New (module->core->isolate, *value->object);
}

GumV8InstructionValue *
_gum_v8_instruction_new_persistent (GumV8Instruction * module)
{
  auto isolate = module->core->isolate;

  auto value = gum_v8_instruction_alloc (module);

  auto template_object = Local<Object>::New (isolate, *module->template_object);
  auto object = template_object->Clone ();
  value->object = new Global<Object> (isolate, object);
  object->SetAlignedPointerInInternalField (0, value);

  return value;
}

void
_gum_v8_instruction_release_persistent (GumV8InstructionValue * value)
{
  gum_v8_instruction_dispose (value);

  value->object->SetWeak (value, gum_v8_instruction_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (value->module->instructions, value);
}

static GumV8InstructionValue *
gum_v8_instruction_alloc (GumV8Instruction * module)
{
  auto value = g_slice_new (GumV8InstructionValue);
  value->object = nullptr;
  value->insn = NULL;
  value->owns_memory = FALSE;
  value->target = NULL;
  value->module = module;

  module->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      GUM_INSTRUCTION_FOOTPRINT_ESTIMATE);

  return value;
}

static void
gum_v8_instruction_dispose (GumV8InstructionValue * self)
{
  if (self->owns_memory && self->insn != NULL)
  {
    cs_free ((cs_insn *) self->insn, 1);
    self->insn = NULL;
  }
}

static void
gum_v8_instruction_free (GumV8InstructionValue * self)
{
  gum_v8_instruction_dispose (self);

  self->module->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      -GUM_INSTRUCTION_FOOTPRINT_ESTIMATE);

  delete self->object;

  g_slice_free (GumV8InstructionValue, self);
}

static gboolean
gum_v8_instruction_check_valid (GumV8InstructionValue * self,
                                Isolate * isolate)
{
  if (self->insn == NULL)
  {
    _gum_v8_throw (isolate, "invalid operation");
    return FALSE;
  }

  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_instruction_parse)
{
  gpointer target;
  if (!_gum_v8_args_parse (args, "p", &target))
    return;

  target = gum_strip_code_pointer (target);

  uint64_t address;
#ifdef HAVE_ARM
  address = GPOINTER_TO_SIZE (target) & ~1;
  cs_option (module->capstone, CS_OPT_MODE,
      (((GPOINTER_TO_SIZE (target) & 1) == 1) ? CS_MODE_THUMB : CS_MODE_ARM) |
      CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN);
#else
  address = GPOINTER_TO_SIZE (target);
#endif

  const gsize max_instruction_size = 16;

  gum_ensure_code_readable (GSIZE_TO_POINTER (address), max_instruction_size);

  cs_insn * insn;
  if (cs_disasm (module->capstone, (uint8_t *) GSIZE_TO_POINTER (address),
      max_instruction_size, address, 1, &insn) == 0)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid instruction");
    return;
  }

  info.GetReturnValue ().Set (
      _gum_v8_instruction_new (module->capstone, insn, TRUE, target, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_address, GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (
      GSIZE_TO_POINTER (self->insn->address), core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_next, GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  auto next = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (self->target) + self->insn->size);

  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (next, core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_size, GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  info.GetReturnValue ().Set (self->insn->size);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_mnemonic,
    GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  info.GetReturnValue ().Set (
      _gum_v8_string_new_ascii (isolate, self->insn->mnemonic));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_op_str, GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  info.GetReturnValue ().Set (
      _gum_v8_string_new_ascii (isolate, self->insn->op_str));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_operands,
                           GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  info.GetReturnValue ().Set (gum_parse_operands (self->insn, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_regs_accessed,
                           GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  cs_regs regs_read, regs_write;
  uint8_t regs_read_count, regs_write_count;

  if (cs_regs_access (module->capstone, self->insn,
        regs_read, &regs_read_count,
        regs_write, &regs_write_count) != 0)
  {
    _gum_v8_throw (isolate, "not yet supported on this architecture");
    return;
  }

  auto result = Object::New (core->isolate);

  _gum_v8_object_set (result, "read",
      gum_parse_regs (regs_read, regs_read_count, module), core);

  _gum_v8_object_set (result, "written",
      gum_parse_regs (regs_write, regs_write_count, module), core);

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_regs_read,
                           GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  auto detail = self->insn->detail;

  info.GetReturnValue ().Set (gum_parse_regs (detail->regs_read,
      detail->regs_read_count, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_regs_written,
                           GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  auto detail = self->insn->detail;

  info.GetReturnValue ().Set (gum_parse_regs (detail->regs_write,
      detail->regs_write_count, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_instruction_get_groups,
                           GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  auto detail = self->insn->detail;

  info.GetReturnValue ().Set (gum_parse_groups (detail->groups,
      detail->groups_count, module));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_instruction_to_string, GumV8InstructionValue)
{
  if (!gum_v8_instruction_check_valid (self, isolate))
    return;

  const cs_insn * insn = self->insn;

  if (*insn->op_str != '\0')
  {
    auto str = g_strconcat (insn->mnemonic, " ", insn->op_str,
        (const gchar *) NULL);
    info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
    g_free (str);
  }
  else
  {
    info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate,
        insn->mnemonic));
  }
}

static void
gum_v8_instruction_on_weak_notify (
    const WeakCallbackInfo<GumV8InstructionValue> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->instructions, self);
}

#if defined (HAVE_I386)

static Local<Array>
gum_parse_operands (const cs_insn * insn,
                    GumV8Instruction * module)
{
  auto core = module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;
  auto x86 = &insn->detail->x86;

  uint8_t op_count = x86->op_count;

  auto elements = Array::New (isolate, op_count);

  for (uint8_t op_index = 0; op_index != op_count; op_index++)
  {
    auto op = &x86->operands[op_index];

    auto element = Object::New (isolate);

    auto type_key = "type";
    auto value_key = "value";

    switch (op->type)
    {
      case X86_OP_REG:
        _gum_v8_object_set_ascii (element, type_key, "reg", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case X86_OP_IMM:
        _gum_v8_object_set_ascii (element, type_key, "imm", core);
        _gum_v8_object_set (element, value_key,
            _gum_v8_int64_new (op->imm, core), core);
        break;
      case X86_OP_MEM:
        _gum_v8_object_set_ascii (element, type_key, "mem", core);
        _gum_v8_object_set (element, value_key,
            gum_x86_parse_memory_operand_value (&op->mem, module), core);
        break;
      default:
        g_assert_not_reached ();
    }

    _gum_v8_object_set_uint (element, "size", op->size, core);

    _gum_v8_object_set_ascii (element, "access",
        gum_access_type_to_string (op->access), core);

    elements->Set (context, op_index, element).Check ();
  }

  return elements;
}

static Local<Object>
gum_x86_parse_memory_operand_value (const x86_op_mem * mem,
                                    GumV8Instruction * module)
{
  auto core = module->core;
  auto capstone = module->capstone;

  auto result = Object::New (core->isolate);

  if (mem->segment != X86_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "segment",
        cs_reg_name (capstone, mem->segment), core);
  }

  if (mem->base != X86_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "base",
        cs_reg_name (capstone, mem->base), core);
  }

  if (mem->index != X86_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "index",
        cs_reg_name (capstone, mem->index), core);
  }

  _gum_v8_object_set_int (result, "scale", mem->scale, core);

  _gum_v8_object_set_int (result, "disp", mem->disp, core);

  return result;
}

#elif defined (HAVE_ARM)

static Local<Array>
gum_parse_operands (const cs_insn * insn,
                    GumV8Instruction * module)
{
  auto core = module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;
  auto arm = &insn->detail->arm;

  uint8_t op_count = arm->op_count;

  auto elements = Array::New (isolate, op_count);

  for (uint8_t op_index = 0; op_index != op_count; op_index++)
  {
    auto op = &arm->operands[op_index];

    auto element = Object::New (isolate);

    auto type_key = "type";
    auto value_key = "value";

    switch (op->type)
    {
      case ARM_OP_REG:
        _gum_v8_object_set_ascii (element, type_key, "reg", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case ARM_OP_IMM:
        _gum_v8_object_set_ascii (element, type_key, "imm", core);
        _gum_v8_object_set_int (element, value_key, op->imm, core);
        break;
      case ARM_OP_MEM:
        _gum_v8_object_set_ascii (element, type_key, "mem", core);
        _gum_v8_object_set (element, value_key,
            gum_arm_parse_memory_operand_value (&op->mem, module), core);
        break;
      case ARM_OP_FP:
        _gum_v8_object_set_ascii (element, type_key, "fp", core);
        _gum_v8_object_set (element, value_key, Number::New (isolate, op->fp),
            core);
        break;
      case ARM_OP_CIMM:
        _gum_v8_object_set_ascii (element, type_key, "cimm", core);
        _gum_v8_object_set_int (element, value_key, op->imm, core);
        break;
      case ARM_OP_PIMM:
        _gum_v8_object_set_ascii (element, type_key, "pimm", core);
        _gum_v8_object_set_int (element, value_key, op->imm, core);
        break;
      case ARM_OP_SETEND:
        _gum_v8_object_set_ascii (element, type_key, "setend", core);
        _gum_v8_object_set_ascii (element, value_key,
            (op->setend == ARM_SETEND_BE) ? "be" : "le", core);
        break;
      case ARM_OP_SYSREG:
        _gum_v8_object_set_ascii (element, type_key, "sysreg", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      default:
        g_assert_not_reached ();
    }

    if (op->shift.type != ARM_SFT_INVALID)
    {
      _gum_v8_object_set (element, "shift",
          gum_arm_parse_shift_details (op, module), core);
    }

    if (op->vector_index != -1)
    {
      _gum_v8_object_set_uint (element, "vectorIndex", op->vector_index, core);
    }

    _gum_v8_object_set (element, "subtracted",
        Boolean::New (isolate, op->subtracted), core);

    _gum_v8_object_set_ascii (element, "access",
        gum_access_type_to_string (op->access), core);

    elements->Set (context, op_index, element).Check ();
  }

  return elements;
}

static Local<Object>
gum_arm_parse_memory_operand_value (const arm_op_mem * mem,
                                    GumV8Instruction * module)
{
  auto core = module->core;
  auto capstone = module->capstone;

  auto result = Object::New (core->isolate);

  if (mem->base != ARM_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "base",
        cs_reg_name (capstone, mem->base), core);
  }

  if (mem->index != ARM_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "index",
        cs_reg_name (capstone, mem->index), core);
  }

  _gum_v8_object_set_int (result, "scale", mem->scale, core);

  _gum_v8_object_set_int (result, "disp", mem->disp, core);

  return result;
}

static Local<Object>
gum_arm_parse_shift_details (const cs_arm_op * op,
                             GumV8Instruction * module)
{
  auto core = module->core;

  auto result = Object::New (core->isolate);

  _gum_v8_object_set_ascii (result, "type",
      gum_arm_shifter_to_string (op->shift.type), core);

  _gum_v8_object_set_uint (result, "value", op->shift.value, core);

  return result;
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

static Local<Array>
gum_parse_operands (const cs_insn * insn,
                    GumV8Instruction * module)
{
  auto core = module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;
  auto arm64 = &insn->detail->arm64;

  uint8_t op_count = arm64->op_count;

  auto elements = Array::New (isolate, op_count);

  for (uint8_t op_index = 0; op_index != op_count; op_index++)
  {
    const cs_arm64_op * op = &arm64->operands[op_index];

    auto element = Object::New (isolate);

    auto type_key = "type";
    auto value_key = "value";

    switch (op->type)
    {
      case ARM64_OP_REG:
        _gum_v8_object_set_ascii (element, type_key, "reg", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case ARM64_OP_IMM:
        _gum_v8_object_set_ascii (element, type_key, "imm", core);
        _gum_v8_object_set (element, value_key,
            _gum_v8_int64_new (op->imm, core), core);
        break;
      case ARM64_OP_MEM:
        _gum_v8_object_set_ascii (element, type_key, "mem", core);
        _gum_v8_object_set (element, value_key,
            gum_arm64_parse_memory_operand_value (&op->mem, module), core);
        break;
      case ARM64_OP_FP:
        _gum_v8_object_set_ascii (element, type_key, "fp", core);
        _gum_v8_object_set (element, value_key, Number::New (isolate, op->fp),
            core);
        break;
      case ARM64_OP_CIMM:
        _gum_v8_object_set_ascii (element, type_key, "cimm", core);
        _gum_v8_object_set (element, value_key,
            _gum_v8_int64_new (op->imm, core), core);
        break;
      case ARM64_OP_REG_MRS:
        _gum_v8_object_set_ascii (element, type_key, "reg-mrs", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case ARM64_OP_REG_MSR:
        _gum_v8_object_set_ascii (element, type_key, "reg-msr", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case ARM64_OP_PSTATE:
        _gum_v8_object_set_ascii (element, type_key, "pstate", core);
        _gum_v8_object_set_uint (element, value_key, op->pstate, core);
        break;
      case ARM64_OP_SYS:
        _gum_v8_object_set_ascii (element, type_key, "sys", core);
        _gum_v8_object_set_uint (element, value_key, op->sys, core);
        break;
      case ARM64_OP_PREFETCH:
        _gum_v8_object_set_ascii (element, type_key, "prefetch", core);
        _gum_v8_object_set_uint (element, value_key, op->prefetch, core);
        break;
      case ARM64_OP_BARRIER:
        _gum_v8_object_set_ascii (element, type_key, "barrier", core);
        _gum_v8_object_set_uint (element, value_key, op->barrier, core);
        break;
      default:
        g_assert_not_reached ();
    }

    if (op->shift.type != ARM64_SFT_INVALID)
    {
      _gum_v8_object_set (element, "shift",
          gum_arm64_parse_shift_details (op, module), core);
    }

    if (op->ext != ARM64_EXT_INVALID)
    {
      _gum_v8_object_set_ascii (element, "ext",
          gum_arm64_extender_to_string (op->ext), core);
    }

    if (op->vas != ARM64_VAS_INVALID)
    {
      _gum_v8_object_set_ascii (element, "vas",
          gum_arm64_vas_to_string (op->vas), core);
    }

    if (op->vector_index != -1)
    {
      _gum_v8_object_set_uint (element, "vectorIndex", op->vector_index, core);
    }

    _gum_v8_object_set_ascii (element, "access",
        gum_access_type_to_string (op->access), core);

    elements->Set (context, op_index, element).Check ();
  }

  return elements;
}

static Local<Object>
gum_arm64_parse_memory_operand_value (const arm64_op_mem * mem,
                                      GumV8Instruction * module)
{
  auto core = module->core;
  auto capstone = module->capstone;

  auto result = Object::New (core->isolate);

  if (mem->base != ARM64_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "base",
        cs_reg_name (capstone, mem->base), core);
  }

  if (mem->index != ARM64_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "index",
        cs_reg_name (capstone, mem->index), core);
  }

  _gum_v8_object_set_int (result, "disp", mem->disp, core);

  return result;
}

static Local<Object>
gum_arm64_parse_shift_details (const cs_arm64_op * op,
                               GumV8Instruction * module)
{
  auto core = module->core;

  auto result = Object::New (core->isolate);

  _gum_v8_object_set_ascii (result, "type",
      gum_arm64_shifter_to_string (op->shift.type), core);

  _gum_v8_object_set_uint (result, "value", op->shift.value, core);

  return result;
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

static Local<Array>
gum_parse_operands (const cs_insn * insn,
                    GumV8Instruction * module)
{
  auto core = module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;
  auto mips = &insn->detail->mips;

  uint8_t op_count = mips->op_count;

  auto elements = Array::New (isolate, op_count);

  for (uint8_t op_index = 0; op_index != op_count; op_index++)
  {
    auto op = &mips->operands[op_index];

    auto element = Object::New (isolate);

    auto type_key = "type";
    auto value_key = "value";

    switch (op->type)
    {
      case MIPS_OP_REG:
        _gum_v8_object_set_ascii (element, type_key, "reg", core);
        _gum_v8_object_set_ascii (element, value_key,
            cs_reg_name (capstone, op->reg), core);
        break;
      case MIPS_OP_IMM:
        _gum_v8_object_set_ascii (element, type_key, "imm", core);
        _gum_v8_object_set_int (element, value_key, op->imm, core);
        break;
      case MIPS_OP_MEM:
        _gum_v8_object_set_ascii (element, type_key, "mem", core);
        _gum_v8_object_set (element, value_key,
            gum_mips_parse_memory_operand_value (&op->mem, module), core);
        break;
      default:
        g_assert_not_reached ();
    }

    elements->Set (context, op_index, element).Check ();
  }

  return elements;
}

static Local<Object>
gum_mips_parse_memory_operand_value (const mips_op_mem * mem,
                                     GumV8Instruction * module)
{
  auto core = module->core;
  auto capstone = module->capstone;

  auto result = Object::New (core->isolate);

  if (mem->base != MIPS_REG_INVALID)
  {
    _gum_v8_object_set_ascii (result, "base",
        cs_reg_name (capstone, mem->base), core);
  }

  _gum_v8_object_set_int (result, "disp", mem->disp, core);

  return result;
}

#endif

static Local<Array>
gum_parse_regs (const uint16_t * regs,
                uint8_t count,
                GumV8Instruction * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;

  auto elements = Array::New (isolate, count);

  for (uint8_t reg_index = 0; reg_index != count; reg_index++)
  {
    auto name = cs_reg_name (capstone, regs[reg_index]);

    elements->Set (context, reg_index,
        _gum_v8_string_new_ascii (isolate, name)).Check ();
  }

  return elements;
}

static Local<Array>
gum_parse_groups (const uint8_t * groups,
                  uint8_t count,
                  GumV8Instruction * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();
  auto capstone = module->capstone;

  auto elements = Array::New (isolate, count);

  for (uint8_t group_index = 0; group_index != count; group_index++)
  {
    auto name = cs_group_name (capstone, groups[group_index]);

    elements->Set (context, group_index,
        _gum_v8_string_new_ascii (isolate, name)).Check ();
  }

  return elements;
}

static const gchar *
gum_access_type_to_string (uint8_t access_type)
{
  switch (access_type)
  {
    case CS_AC_INVALID:            return "";
    case CS_AC_READ:               return "r";
    case CS_AC_WRITE:              return "w";
    case CS_AC_READ | CS_AC_WRITE: return "rw";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}
