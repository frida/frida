from __future__ import unicode_literals, print_function
from pathlib import Path
import re
import sys


def main(argv):
    output_dir, source_dir = [Path(p) for p in argv[1:]]
    generate_and_write_bindings(output_dir, source_dir)

def generate_and_write_bindings(output_dir: Path, source_dir: Path):
    binding_params = [
        ("writer", { 'ignore': ['new', 'ref', 'unref', 'init', 'clear', 'reset',
                                'set_target_cpu', 'set_target_abi', 'set_target_os',
                                'cur', 'offset', 'flush', 'get_cpu_register_for_nth_argument'] }),
        ("relocator", { 'ignore': ['new', 'ref', 'unref', 'init', 'clear', 'reset',
                                   'read_one', 'is_eob_instruction', 'eob', 'eoi', 'can_relocate'] }),
    ]

    flavor_combos = [
        ("x86", "x86"),
        ("arm", "arm"),
        ("arm", "thumb"),
        ("arm64", "arm64"),
        ("mips", "mips"),
    ]

    tsds = {}
    docs = {}

    for name, options in binding_params:
        for filename, code in generate_umbrellas(name, flavor_combos).items():
            (output_dir / filename).write_text(code, encoding='utf-8')

        for arch, flavor in flavor_combos:
            api_header_path = source_dir / f"arch-{arch}" / f"gum{flavor}{name}.h"
            api_header = api_header_path.read_text(encoding='utf-8').replace("\r", "")

            bindings = generate_bindings(name, arch, flavor, api_header, options)

            for filename, code in bindings.code.items():
                (output_dir / filename).write_text(code, encoding='utf-8')

            tsds.update(bindings.tsds)
            docs.update(bindings.docs)

    tsd_sections = []
    doc_sections = []
    for arch, flavor in flavor_combos:
        for name, options in binding_params:
            tsd_sections.append(tsds["{0}-{1}.d.ts".format(flavor, name)])
            doc_sections.append(docs["{0}-{1}.md".format(flavor, name)])
        if flavor != "arm":
            tsd_sections.append(tsds["{0}-enums.d.ts".format(arch)])
            doc_sections.append(docs["{0}-enums.md".format(arch)])

    tsd_source = "\n\n".join(tsd_sections)
    (output_dir / "api-types.d.ts").write_text(tsd_source, encoding='utf-8')

    api_reference = "\n\n".join(doc_sections)
    (output_dir / "api-reference.md").write_text(api_reference, encoding='utf-8')

def generate_umbrellas(name, flavor_combos):
    umbrellas = {}
    for runtime in ["quick", "v8"]:
        for section in ["", "-fields", "-methods", "-init", "-dispose"]:
            filename, code = generate_umbrella(runtime, name, section, flavor_combos)
            umbrellas[filename] = code
    return umbrellas

def generate_umbrella(runtime, name, section, flavor_combos):
    lines = []

    arch_defines = {
        "x86": "HAVE_I386",
        "arm": "HAVE_ARM",
        "arm64": "HAVE_ARM64",
        "mips": "HAVE_MIPS",
    }

    current_arch = None
    for arch, flavor in flavor_combos:
        if arch != current_arch:
            if current_arch is not None:
                lines.extend([
                    "#endif",
                    "",
                ])
            lines.extend([
                "#ifdef " + arch_defines[arch],
                "",
            ])
            current_arch = arch

        lines.append("# include \"gum{0}code{1}{2}-{3}.inc\"".format(runtime, name, section, flavor))

        if section == "-methods":
            if flavor == "thumb":
                lines.extend(generate_alias_definitions("special", runtime, name, flavor))
            else:
                lines.extend(generate_alias_definitions("default", runtime, name, flavor))
                if flavor != "arm":
                    lines.extend(generate_alias_definitions("special", runtime, name, flavor))

    lines.append("#endif")

    filename = "gum{0}code{1}{2}.inc".format(runtime, name, section)
    code = "\n".join(lines)

    return (filename, code)

def generate_alias_definitions(alias, runtime, name, flavor):
    alias_function_prefix = "gum_{0}_{1}_{2}".format(runtime, alias, name)
    wrapper_function_prefix = "gum_{0}_{1}_{2}".format(runtime, flavor, name)
    impl_function_prefix = "gum_{0}_{1}".format(flavor, name)

    params = {
        "name_uppercase": name.upper(),
        "alias_class_name": to_camel_case("{0}_{1}".format(flavor, name), start_high=True),
        "alias_field_prefix": "{0}_{1}".format(flavor, name),
        "alias_struct_name": to_camel_case(alias_function_prefix, start_high=True),
        "alias_function_prefix": alias_function_prefix,
        "wrapper_macro_prefix": "GUM_{0}_{1}_{2}".format(runtime.upper(), alias.upper(), name.upper()),
        "wrapper_struct_name": to_camel_case(wrapper_function_prefix, start_high=True),
        "wrapper_function_prefix": wrapper_function_prefix,
        "impl_struct_name": to_camel_case(impl_function_prefix, start_high=True),
        "persistent_suffix": "_persistent" if runtime == "v8" else ""
    }

    return """
#define {wrapper_macro_prefix}_CLASS_NAME "{alias_class_name}"
#define {wrapper_macro_prefix}_FIELD {alias_field_prefix}

typedef {wrapper_struct_name} {alias_struct_name};
typedef {impl_struct_name} {alias_struct_name}Impl;

#define _{alias_function_prefix}_new{persistent_suffix} _{wrapper_function_prefix}_new{persistent_suffix}
#define _{alias_function_prefix}_release{persistent_suffix} _{wrapper_function_prefix}_release{persistent_suffix}
#define _{alias_function_prefix}_init _{wrapper_function_prefix}_init
#define _{alias_function_prefix}_finalize _{wrapper_function_prefix}_finalize
#define _{alias_function_prefix}_gc_mark _{wrapper_function_prefix}_gc_mark
#define _{alias_function_prefix}_reset _{wrapper_function_prefix}_reset
""".format(**params).split("\n")

class Bindings(object):
    def __init__(self, code, tsds, docs):
        self.code = code
        self.tsds = tsds
        self.docs = docs

def generate_bindings(name, arch, flavor, api_header, options):
    api = parse_api(name, arch, flavor, api_header, options)

    code = {}
    code.update(generate_quick_bindings(name, arch, flavor, api))
    code.update(generate_v8_bindings(name, arch, flavor, api))

    tsds = generate_tsds(name, arch, flavor, api)

    docs = generate_docs(name, arch, flavor, api)

    return Bindings(code, tsds, docs)

def generate_quick_bindings(name, arch, flavor, api):
    component = Component(name, arch, flavor, "quick")
    return {
        "gumquickcode{0}-{1}.inc".format(name, flavor): generate_quick_wrapper_code(component, api),
        "gumquickcode{0}-fields-{1}.inc".format(name, flavor): generate_quick_fields(component),
        "gumquickcode{0}-methods-{1}.inc".format(name, flavor): generate_quick_methods(component),
        "gumquickcode{0}-init-{1}.inc".format(name, flavor): generate_quick_init_code(component),
        "gumquickcode{0}-dispose-{1}.inc".format(name, flavor): generate_quick_dispose_code(component),
    }

def generate_quick_wrapper_code(component, api):
    lines = [
        "/* Auto-generated, do not edit. */",
        "",
        "#include <string.h>",
    ]

    conversion_decls, conversion_code = generate_conversion_methods(component, generate_quick_enum_parser)
    if len(conversion_decls) > 0:
        lines.append("")
        lines.extend(conversion_decls)

    lines.append("")

    lines.extend(generate_quick_base_methods(component))

    for method in api.instance_methods:
        args = method.args

        is_put_array = method.is_put_array
        if method.is_put_call:
            array_item_type = "GumArgument"
            array_item_parse_logic = generate_quick_parse_call_arg_array_element(component)
        elif method.is_put_regs:
            array_item_type = api.native_register_type
            array_item_parse_logic = generate_quick_parse_register_array_element(component)

        lines.extend([
            "GUMJS_DEFINE_FUNCTION ({0}_{1})".format(component.gumjs_function_prefix, method.name),
            "{",
            "  {0} * parent;".format(component.module_struct_name),
            "  {0} * self;".format(component.wrapper_struct_name),
        ])

        for arg in args:
            type_raw = arg.type_raw
            if type_raw == "$array":
                type_raw = "JSValue"
            lines.append("  {0} {1};".format(type_raw, arg.name_raw))
            converter = arg.type_converter
            if converter is not None:
                if converter == "bytes":
                    lines.extend([
                        "  const guint8 * {0};".format(arg.name),
                        "  gsize {0}_size;".format(arg.name)
                    ])
                elif converter == "label":
                    lines.append("  gconstpointer {0};".format(arg.name))
                else:
                    lines.append("  {0} {1};".format(arg.type, arg.name))
        if is_put_array:
            lines.extend([
                "  guint items_length, items_index;",
                "  {0} * items;".format(array_item_type),
                "  JSValue element_val = JS_NULL;",
                "  const char * element_str = NULL;",
            ])

        if method.return_type == "void":
            return_capture = ""
        else:
            lines.append("  {0} result;".format(method.return_type))
            return_capture = "result = "

        lines.extend([
            "",
            "  parent = gumjs_get_parent_module (core);",
            "",
            "  if (!_{0}_get (ctx, this_val, parent, &self))".format(component.wrapper_function_prefix),
            "    goto propagate_exception;",
        ])

        if len(args) > 0:
            arglist_signature = "".join([arg.type_format for arg in args])
            arglist_pointers = ", ".join(["&" + arg.name_raw for arg in args])

            lines.extend([
                "",
                "  if (!_gum_quick_args_parse (args, \"{0}\", {1}))".format(arglist_signature, arglist_pointers),
                "    goto propagate_exception;",
            ])

        args_needing_conversion = [arg for arg in args if arg.type_converter is not None]
        if len(args_needing_conversion) > 0:
            lines.append("")
            for arg in args_needing_conversion:
                converter = arg.type_converter
                if converter == "label":
                    lines.append("  {value} = {wrapper_function_prefix}_resolve_label (self, {value_raw});".format(
                        value=arg.name,
                        value_raw=arg.name_raw,
                        wrapper_function_prefix=component.wrapper_function_prefix))
                elif converter == "address":
                    lines.append("  {value} = GUM_ADDRESS ({value_raw});".format(
                        value=arg.name,
                        value_raw=arg.name_raw))
                elif converter == "bytes":
                    lines.append("  {value} = g_bytes_get_data ({value_raw}, &{value}_size);".format(
                        value=arg.name,
                        value_raw=arg.name_raw))
                else:
                    lines.append("  if (!_gum_parse_{arch}_{type} (ctx, {value_raw}, &{value}))\n    goto propagate_exception;".format(
                        value=arg.name,
                        value_raw=arg.name_raw,
                        arch=component.arch,
                        type=arg.type_converter))

        if is_put_array:
            lines.extend(generate_quick_parse_array_elements(array_item_type, array_item_parse_logic).split("\n"))

        impl_function_name = "{0}_{1}".format(component.impl_function_prefix, method.name)

        arglist = ["self->impl"]
        if method.needs_calling_convention_arg:
            arglist.append("GUM_CALL_CAPI")
        for arg in args:
            if arg.type_converter == "bytes":
                arglist.extend([arg.name, arg.name + "_size"])
            else:
                arglist.append(arg.name)
        if is_put_array:
            impl_function_name += "_array"
            arglist.insert(len(arglist) - 1, "items_length")

        lines.extend([
            "",
            "  {0}{1} ({2});".format(return_capture, impl_function_name, ", ".join(arglist))
        ])

        error_targets = []

        if method.return_type == "gboolean" and method.name.startswith("put_"):
            lines.extend([
                "",
                "  if (!result)",
                "    goto invalid_argument;",
                "",
                "  return JS_UNDEFINED;",
            ])
            error_targets.extend([
                "invalid_argument:",
                "  {",
                "    _gum_quick_throw_literal (ctx, \"invalid argument\");",
                "    goto propagate_exception;",
                "  }",
            ])
        elif method.return_type == "void":
            lines.append("")
            lines.append("  return JS_UNDEFINED;")
        else:
            lines.append("")
            if method.return_type == "gboolean":
                lines.append("  return JS_NewBool (ctx, result);")
            elif method.return_type == "guint":
                lines.append("  return JS_NewInt64 (ctx, result);")
            elif method.return_type == "gpointer":
                lines.append("  return _gum_quick_native_pointer_new (ctx, result, core);")
            elif method.return_type == "GumAddress":
                lines.append("  return _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (result), core);")
            elif method.return_type == "cs_insn *":
                target = "\n".join([
                    "self->impl->input_start + (result->address -",
                    "          (self->impl->input_pc -",
                    "            (self->impl->input_cur - self->impl->input_start)))",
                ])
                if component.flavor == "thumb":
                    target = "GSIZE_TO_POINTER (GPOINTER_TO_SIZE ({0}) | 1)".format(target)
                lines.extend([
                    "  if (result != NULL)",
                    "  {",
                    "    return _gum_quick_instruction_new (ctx, result, FALSE,",
                    "        {0},".format(target),
                    "        self->impl->capstone, parent->instruction, NULL);",
                    "  }",
                    "  else",
                    "  {",
                    "    return JS_NULL;",
                    "  }",
                ])
            else:
                raise ValueError("Unsupported return type: {0}".format(method.return_type))

        lines.append("")
        lines.extend(error_targets)
        lines.extend([
            "propagate_exception:",
            "  {",
        ])
        if is_put_array:
            lines.extend([
                "    JS_FreeCString (ctx, element_str);",
                "    JS_FreeValue (ctx, element_val);",
                "",
            ])
        lines.extend([
            "    return JS_EXCEPTION;",
            "  }",
            "}",
            ""
        ])

    prefix = component.gumjs_function_prefix
    lines.extend([
        "static const JSClassDef {0}_def =".format(prefix),
        "{",
        "  .class_name = \"{0}\",".format(component.gumjs_class_name),
        "  .finalizer = {0}_finalize,".format(prefix),
        "  .gc_mark = {0}_gc_mark,".format(prefix),
        "};",
        "",
        "static const JSCFunctionListEntry {0}_entries[] =".format(prefix),
        "{",
    ])
    if component.name == "writer":
        lines.extend([
            "  JS_CGETSET_DEF (\"base\", {0}_get_base, NULL),".format(prefix),
            "  JS_CGETSET_DEF (\"code\", {0}_get_code, NULL),".format(prefix),
            "  JS_CGETSET_DEF (\"pc\", {0}_get_pc, NULL),".format(prefix),
            "  JS_CGETSET_DEF (\"offset\", {0}_get_offset, NULL),".format(prefix),
            "  JS_CFUNC_DEF (\"reset\", 0, {0}_reset),".format(prefix),
            "  JS_CFUNC_DEF (\"dispose\", 0, {0}_dispose),".format(prefix),
            "  JS_CFUNC_DEF (\"flush\", 0, {0}_flush),".format(prefix),
        ])
    elif component.name == "relocator":
        lines.extend([
            "  JS_CGETSET_DEF (\"input\", {0}_get_input, NULL),".format(prefix),
            "  JS_CGETSET_DEF (\"eob\", {0}_get_eob, NULL),".format(prefix),
            "  JS_CGETSET_DEF (\"eoi\", {0}_get_eoi, NULL),".format(prefix),
            "  JS_CFUNC_DEF (\"reset\", 0, {0}_reset),".format(prefix),
            "  JS_CFUNC_DEF (\"dispose\", 0, {0}_dispose),".format(prefix),
            "  JS_CFUNC_DEF (\"readOne\", 0, {0}_read_one),".format(prefix),
        ])

    for method in api.instance_methods:
        lines.append("  JS_CFUNC_DEF (\"{0}\", 0, {1}_{2}),".format(
            method.name_js,
            component.gumjs_function_prefix,
            method.name
        ))

    lines.extend([
        "};",
        ""
    ])

    lines.extend(conversion_code)

    return "\n".join(lines)

def generate_quick_parse_array_elements(item_type, parse_item):
    return """
  if (!_gum_quick_array_get_length (ctx, items_value, core, &items_length))
    goto propagate_exception;
  items = g_newa ({item_type}, items_length);

  for (items_index = 0; items_index != items_length; items_index++)
  {{
    {item_type} * item = &items[items_index];

    element_val = JS_GetPropertyUint32 (ctx, items_value, items_index);
    if (JS_IsException (element_val))
      goto propagate_exception;
{parse_item}

    JS_FreeValue (ctx, element_val);
    element_val = JS_NULL;
  }}""".format(item_type=item_type, parse_item=parse_item)

def generate_quick_parse_call_arg_array_element(component):
    return """
    if (JS_IsString (element_val))
    {{
      {register_type} r;

      element_str = JS_ToCString (ctx, element_val);
      if (element_str == NULL)
        goto propagate_exception;

      if (!_gum_parse_{arch}_register (ctx, element_str, &r))
        goto propagate_exception;

      item->type = GUM_ARG_REGISTER;
      item->value.reg = r;

      JS_FreeCString (ctx, element_str);
      element_str = NULL;
    }}
    else
    {{
      gpointer ptr;

      if (!_gum_quick_native_pointer_parse (ctx, element_val, core, &ptr))
        goto propagate_exception;

      item->type = GUM_ARG_ADDRESS;
      item->value.address = GUM_ADDRESS (ptr);
    }}""".format(arch=component.arch, register_type=component.register_type)

def generate_quick_parse_register_array_element(component):
    return """
    if (!JS_IsString (element_val))
      goto invalid_argument;

    {{
      {register_type} reg;

      element_str = JS_ToCString (ctx, element_val);
      if (element_str == NULL)
        goto propagate_exception;

      if (!_gum_parse_{arch}_register (ctx, element_str, &reg))
        goto propagate_exception;

      *item = reg;

      JS_FreeCString (ctx, element_str);
      element_str = NULL;
    }}""".format(arch=component.arch, register_type=component.register_type)

def generate_quick_fields(component):
    return """  JSClassID {flavor}_{name}_class;
  JSValue {flavor}_{name}_proto;""".format(**component.__dict__)

def generate_quick_methods(component):
    params = dict(component.__dict__)

    extra_fields = ""
    if component.name == "writer":
        extra_fields = "\n  GHashTable * labels;"
    if component.name == "relocator":
        extra_fields = "\n  GumQuickInstructionValue * input;"

    params["extra_fields"] = extra_fields

    template = """\
#include <gum/arch-{arch}/gum{flavor}{name}.h>

typedef struct _{wrapper_struct_name} {wrapper_struct_name};

struct _{wrapper_struct_name}
{{
  JSValue wrapper;
  {impl_struct_name} * impl;{extra_fields}
  JSContext * ctx;
}};

G_GNUC_INTERNAL JSValue _gum_quick_{flavor}_{name}_new (JSContext * ctx, {impl_struct_name} * impl, {module_struct_name} * parent, {wrapper_struct_name} ** {flavor}_{name});
G_GNUC_INTERNAL gboolean _gum_quick_{flavor}_{name}_get (JSContext * ctx, JSValue val, {module_struct_name} * parent, {wrapper_struct_name} ** writer);

G_GNUC_INTERNAL void _gum_quick_{flavor}_{name}_init ({wrapper_struct_name} * self, JSContext * ctx, {module_struct_name} * parent);
G_GNUC_INTERNAL void _gum_quick_{flavor}_{name}_finalize ({wrapper_struct_name} * self);
G_GNUC_INTERNAL void _gum_quick_{flavor}_{name}_gc_mark (JSRuntime * rt, JSValueConst val, JS_MarkFunc * mark_func);
G_GNUC_INTERNAL void _gum_quick_{flavor}_{name}_reset ({wrapper_struct_name} * self, {impl_struct_name} * impl);
"""
    return template.format(**params) + generate_quick_enum_parser_declarations(component)

def generate_quick_init_code(component):
    return """\
  _gum_quick_create_class (ctx, &{gumjs_function_prefix}_def, core,
      &self->{gumjs_field_prefix}_class, &proto);
  self->{gumjs_field_prefix}_proto = JS_DupValue (ctx, proto);
  ctor = JS_NewCFunction2 (ctx, {gumjs_function_prefix}_construct,
      {gumjs_function_prefix}_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, {gumjs_function_prefix}_entries,
      G_N_ELEMENTS ({gumjs_function_prefix}_entries));
  JS_DefinePropertyValueStr (ctx, ns, {gumjs_function_prefix}_def.class_name, ctor,
      JS_PROP_C_W_E);
""".format(**component.__dict__)

def generate_quick_dispose_code(component):
    return """\
  JS_FreeValue (ctx, self->{gumjs_field_prefix}_proto);
  self->{gumjs_field_prefix}_proto = JS_NULL;
""".format(**component.__dict__)

def generate_quick_base_methods(component):
    if component.name == "writer":
        return generate_quick_writer_base_methods(component)
    elif component.name == "relocator":
        return generate_quick_relocator_base_methods(component)

def generate_quick_writer_base_methods(component):
    template = """\
static {wrapper_struct_name} * {wrapper_function_prefix}_alloc (JSContext * ctx, {module_struct_name} * module);
static void {wrapper_function_prefix}_dispose ({wrapper_struct_name} * self);
static gboolean {gumjs_function_prefix}_parse_constructor_args (GumQuickArgs * args,
    gpointer * code_address, GumAddress * pc, gboolean * pc_specified);

JSValue
_gum_quick_{flavor}_writer_new (
    JSContext * ctx,
    {impl_struct_name} * impl,
    {module_struct_name} * parent,
    {wrapper_struct_name} ** writer)
{{
  JSValue wrapper;
  {wrapper_struct_name} * w;

  wrapper = JS_NewObjectClass (ctx, parent->{flavor}_writer_class);

  w = {wrapper_function_prefix}_alloc (ctx, parent);
  w->impl = (impl != NULL) ? {impl_function_prefix}_ref (impl) : NULL;

  JS_SetOpaque (wrapper, w);

  if (writer != NULL)
    *writer = w;

  return wrapper;
}}

gboolean
_gum_quick_{flavor}_writer_get (
    JSContext * ctx,
    JSValue val,
    {module_struct_name} * parent,
    {wrapper_struct_name} ** writer)
{{
  {wrapper_struct_name} * w;

  if (!_gum_quick_unwrap (ctx, val, parent->{flavor}_writer_class, parent->core,
      (gpointer *) &w))
    return FALSE;

  if (w->impl == NULL)
  {{
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }}

  *writer = w;
  return TRUE;
}}

void
_{wrapper_function_prefix}_init (
    {wrapper_struct_name} * self,
    JSContext * ctx,
    {module_struct_name} * parent)
{{
  self->wrapper = JS_NULL;
  self->impl = NULL;
  self->ctx = ctx;
  self->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}}

void
_{wrapper_function_prefix}_finalize ({wrapper_struct_name} * self)
{{
  _gum_quick_{flavor}_writer_reset (self, NULL);
  g_hash_table_unref (self->labels);
}}

void
_{wrapper_function_prefix}_reset (
    {wrapper_struct_name} * self,
    {impl_struct_name} * impl)
{{
  if (impl != NULL)
    {impl_function_prefix}_ref (impl);
  if (self->impl != NULL)
    {impl_function_prefix}_unref (self->impl);
  self->impl = impl;

  g_hash_table_remove_all (self->labels);
}}

static {wrapper_struct_name} *
{wrapper_function_prefix}_alloc (JSContext * ctx,
                                 {module_struct_name} * module)
{{
  {wrapper_struct_name} * writer;

  writer = g_slice_new ({wrapper_struct_name});
  _{wrapper_function_prefix}_init (writer, ctx, module);

  return writer;
}}

static void
{wrapper_function_prefix}_dispose ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);
}}

static void
{wrapper_function_prefix}_free ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_finalize (self);

  g_slice_free ({wrapper_struct_name}, self);
}}

{label_resolver}

GUMJS_DEFINE_CONSTRUCTOR ({gumjs_function_prefix}_construct)
{{
  {module_struct_name} * parent;
  JSValue wrapper;
  gpointer code_address;
  GumAddress pc;
  gboolean pc_specified;
  JSValue proto;
  {wrapper_struct_name} * writer;

  parent = gumjs_get_parent_module (core);

  if (!{gumjs_function_prefix}_parse_constructor_args (args, &code_address, &pc,
      &pc_specified))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, parent->{flavor}_writer_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  writer = {wrapper_function_prefix}_alloc (ctx, parent);
  writer->wrapper = wrapper;
  writer->impl = {impl_function_prefix}_new (code_address);
  writer->impl->flush_on_destroy = FALSE;
  if (pc_specified)
    writer->impl->pc = pc;

  JS_SetOpaque (wrapper, writer);

  return wrapper;
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_reset)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;
  gpointer code_address;
  GumAddress pc;
  gboolean pc_specified;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (!{gumjs_function_prefix}_parse_constructor_args (args, &code_address, &pc,
      &pc_specified))
    return JS_EXCEPTION;

  {impl_function_prefix}_flush (self->impl);

  {impl_function_prefix}_reset (self->impl, code_address);
  if (pc_specified)
    self->impl->pc = pc;

  g_hash_table_remove_all (self->labels);

  return JS_UNDEFINED;
}}

static gboolean
{gumjs_function_prefix}_parse_constructor_args (
    GumQuickArgs * args,
    gpointer * code_address,
    GumAddress * pc,
    gboolean * pc_specified)
{{
  JSContext * ctx = args->ctx;
  JSValue options;

  options = JS_NULL;
  if (!_gum_quick_args_parse (args, "p|O", code_address, &options))
    return FALSE;

  *pc = 0;
  *pc_specified = FALSE;

  if (!JS_IsNull (options))
  {{
    GumQuickCore * core = args->core;
    JSValue val;

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, pc));
    if (JS_IsException (val))
      return FALSE;

    if (!JS_IsUndefined (val))
    {{
      gboolean valid;
      gpointer p;

      valid = _gum_quick_native_pointer_get (ctx, val, core, &p);
      JS_FreeValue (ctx, val);
      if (!valid)
        return FALSE;

      *pc = GUM_ADDRESS (p);
      *pc_specified = TRUE;
    }}

  }}

  return TRUE;
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_dispose)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  {impl_function_prefix}_flush (self->impl);

  {wrapper_function_prefix}_dispose (self);

  return JS_UNDEFINED;
}}

GUMJS_DEFINE_FINALIZER ({gumjs_function_prefix}_finalize)
{{
  {wrapper_struct_name} * w;

  w = JS_GetOpaque (val, gumjs_get_parent_module (core)->{flavor}_writer_class);
  if (w == NULL)
    return;

  {wrapper_function_prefix}_free (w);
}}

GUMJS_DEFINE_GC_MARKER ({gumjs_function_prefix}_gc_mark)
{{
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_flush)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;
  gboolean success;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  success = {impl_function_prefix}_flush (self->impl);
  if (!success)
    return _gum_quick_throw_literal (ctx, "unable to resolve references");

  return JS_UNDEFINED;
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_base)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, self->impl->base, core);
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_code)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, self->impl->code, core);
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_pc)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (self->impl->pc),
      core);
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_offset)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewInt32 (ctx, {impl_function_prefix}_offset (self->impl));
}}
"""
    params = dict(component.__dict__)

    params["label_resolver"] = """static gconstpointer
{wrapper_function_prefix}_resolve_label ({wrapper_struct_name} * self,
    const gchar * str)
{{
  gchar * label = g_hash_table_lookup (self->labels, str);
  if (label != NULL)
    return label;

  label = g_strdup (str);
  g_hash_table_add (self->labels, label);
  return label;
}}""".format(**params)

    return template.format(**params).split("\n")

def generate_quick_relocator_base_methods(component):
    template = """\
static {wrapper_struct_name} * {wrapper_function_prefix}_alloc (JSContext * ctx, {module_struct_name} * module);
static void {wrapper_function_prefix}_dispose ({wrapper_struct_name} * self);
static gboolean {gumjs_function_prefix}_parse_constructor_args (GumQuickArgs * args,
    gconstpointer * input_code, {writer_wrapper_struct_name} ** writer, {module_struct_name} * parent);

JSValue
_gum_quick_{flavor}_relocator_new (
    JSContext * ctx,
    {impl_struct_name} * impl,
    {module_struct_name} * parent,
    {wrapper_struct_name} ** relocator)
{{
  JSValue wrapper;
  {wrapper_struct_name} * r;

  wrapper = JS_NewObjectClass (ctx, parent->{flavor}_relocator_class);

  r = {wrapper_function_prefix}_alloc (ctx, parent);
  r->impl = (impl != NULL) ? {impl_function_prefix}_ref (impl) : NULL;

  JS_SetOpaque (wrapper, r);

  if (relocator != NULL)
    *relocator = r;

  return wrapper;
}}

gboolean
_gum_quick_{flavor}_relocator_get (
    JSContext * ctx,
    JSValue val,
    {module_struct_name} * parent,
    {wrapper_struct_name} ** relocator)
{{
  {wrapper_struct_name} * r;

  if (!_gum_quick_unwrap (ctx, val, parent->{flavor}_relocator_class, parent->core,
      (gpointer *) &r))
    return FALSE;

  if (r->impl == NULL)
  {{
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }}

  *relocator = r;
  return TRUE;
}}

void
_{wrapper_function_prefix}_init (
    {wrapper_struct_name} * self,
    JSContext * ctx,
    {module_struct_name} * parent)
{{
  self->wrapper = JS_NULL;
  self->impl = NULL;
  _gum_quick_instruction_new (ctx, NULL, TRUE, NULL, 0, parent->instruction,
      &self->input);
  self->ctx = ctx;
}}

void
_{wrapper_function_prefix}_finalize ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);

  JS_FreeValue (self->ctx, self->input->wrapper);
}}

void
_{wrapper_function_prefix}_reset (
    {wrapper_struct_name} * self,
    {impl_struct_name} * impl)
{{
  if (impl != NULL)
    {impl_function_prefix}_ref (impl);
  if (self->impl != NULL)
    {impl_function_prefix}_unref (self->impl);
  self->impl = impl;

  self->input->insn = NULL;
}}

static {wrapper_struct_name} *
{wrapper_function_prefix}_alloc (JSContext * ctx,
                                 {module_struct_name} * parent)
{{
  {wrapper_struct_name} * relocator;

  relocator = g_slice_new ({wrapper_struct_name});
  _{wrapper_function_prefix}_init (relocator, ctx, parent);

  return relocator;
}}

static void
{wrapper_function_prefix}_dispose ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);
}}

static void
{wrapper_function_prefix}_free ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_finalize (self);

  g_slice_free ({wrapper_struct_name}, self);
}}

GUMJS_DEFINE_CONSTRUCTOR ({gumjs_function_prefix}_construct)
{{
  {module_struct_name} * parent;
  JSValue wrapper;
  gconstpointer input_code;
  {writer_wrapper_struct_name} * writer;
  JSValue proto;
  {wrapper_struct_name} * relocator;

  parent = gumjs_get_parent_module (core);

  if (!{gumjs_function_prefix}_parse_constructor_args (args, &input_code, &writer,
      parent))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, parent->{flavor}_relocator_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  relocator = {wrapper_function_prefix}_alloc (ctx, parent);
  relocator->wrapper = wrapper;
  relocator->impl = {impl_function_prefix}_new (input_code, writer->impl);

  JS_SetOpaque (wrapper, relocator);

  return wrapper;
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_reset)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;
  gconstpointer input_code;
  {writer_wrapper_struct_name} * writer;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (!{gumjs_function_prefix}_parse_constructor_args (args, &input_code, &writer,
      parent))
    return JS_EXCEPTION;

  {impl_function_prefix}_reset (self->impl, input_code, writer->impl);

  self->input->insn = NULL;

  return JS_UNDEFINED;
}}

static gboolean
{gumjs_function_prefix}_parse_constructor_args (
    GumQuickArgs * args,
    gconstpointer * input_code,
    {writer_wrapper_struct_name} ** writer,
    {module_struct_name} * parent)
{{
  JSValue writer_object;

  if (!_gum_quick_args_parse (args, "pO", input_code, &writer_object))
    return FALSE;

  if (!_gum_quick_{flavor}_writer_get (args->ctx, writer_object, parent->writer,
      writer))
    return FALSE;

  return TRUE;
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_dispose)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  {wrapper_function_prefix}_dispose (self);

  return JS_UNDEFINED;
}}

GUMJS_DEFINE_FINALIZER ({gumjs_function_prefix}_finalize)
{{
  {wrapper_struct_name} * r;

  r = JS_GetOpaque (val, gumjs_get_parent_module (core)->{flavor}_relocator_class);
  if (r == NULL)
    return;

  {wrapper_function_prefix}_free (r);
}}

GUMJS_DEFINE_GC_MARKER ({gumjs_function_prefix}_gc_mark)
{{
  {wrapper_struct_name} * r;

  r = JS_GetOpaque (val, gumjs_get_parent_module (core)->{flavor}_relocator_class);
  if (r == NULL)
    return;

  JS_MarkValue (rt, r->input->wrapper, mark_func);
}}

GUMJS_DEFINE_FUNCTION ({gumjs_function_prefix}_read_one)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;
  guint n_read;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  n_read = {impl_function_prefix}_read_one (self->impl, &self->input->insn);
  if (n_read != 0)
  {{
    self->input->target = {get_input_target_expression};
  }}

  return JS_NewInt32 (ctx, n_read);
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_input)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (self->input->insn == NULL)
    return JS_NULL;

  return JS_DupValue (ctx, self->input->wrapper);
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_eob)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, {impl_function_prefix}_eob (self->impl));
}}

GUMJS_DEFINE_GETTER ({gumjs_function_prefix}_get_eoi)
{{
  {module_struct_name} * parent;
  {wrapper_struct_name} * self;

  parent = gumjs_get_parent_module (core);

  if (!_{wrapper_function_prefix}_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, {impl_function_prefix}_eoi (self->impl));
}}
"""

    target = "self->impl->input_cur - self->input->insn->size"
    if component.flavor == "thumb":
        target = "GSIZE_TO_POINTER (GPOINTER_TO_SIZE ({0}) | 1)".format(target)

    params = {
        "writer_wrapper_struct_name": component.wrapper_struct_name.replace("Relocator", "Writer"),
        "get_input_target_expression": target,
    }
    params.update(component.__dict__)

    return template.format(**params).split("\n")

def generate_quick_enum_parser(name, type, prefix, values):
    common_decls, common_code = generate_enum_parser(name, type, prefix, values)

    params = {
        'name': name,
        'result_identifier': name.split("_")[-1].replace("register", "reg"),
        'description': name.replace("_", " "),
        'type': type,
    }

    decls = common_decls

    code = """\
gboolean
_gum_parse_{name} (
    JSContext * ctx,
    const gchar * name,
    {type} * {result_identifier})
{{
  if (!gum_try_parse_{name} (name, {result_identifier}))
  {{
    _gum_quick_throw_literal (ctx, "invalid {description}");
    return FALSE;
  }}

  return TRUE;
}}
""".format(**params).split("\n") + common_code

    return (decls, code)

def generate_v8_bindings(name, arch, flavor, api):
    component = Component(name, arch, flavor, "v8")
    return {
        "gumv8code{0}-{1}.inc".format(name, flavor): generate_v8_wrapper_code(component, api),
        "gumv8code{0}-fields-{1}.inc".format(name, flavor): generate_v8_fields(component),
        "gumv8code{0}-methods-{1}.inc".format(name, flavor): generate_v8_methods(component),
        "gumv8code{0}-init-{1}.inc".format(name, flavor): generate_v8_init_code(component),
        "gumv8code{0}-dispose-{1}.inc".format(name, flavor): generate_v8_dispose_code(component),
    }

def generate_v8_wrapper_code(component, api):
    lines = [
        "/* Auto-generated, do not edit. */",
        "",
        "#include <string>",
        "#include <string.h>",
    ]

    conversion_decls, conversion_code = generate_conversion_methods(component, generate_v8_enum_parser)
    if len(conversion_decls) > 0:
        lines.append("")
        lines.extend(conversion_decls)

    lines.append("")

    lines.extend(generate_v8_base_methods(component))

    for method in api.instance_methods:
        args = method.args

        is_put_array = method.is_put_array
        if method.is_put_call:
            array_item_type = "GumArgument"
            array_item_parse_logic = generate_v8_parse_call_arg_array_element(component, api)
        elif method.is_put_regs:
            array_item_type = api.native_register_type
            array_item_parse_logic = generate_v8_parse_register_array_element(component, api)

        lines.extend([
            "GUMJS_DEFINE_CLASS_METHOD ({0}_{1}, {2})".format(component.gumjs_function_prefix, method.name, component.wrapper_struct_name),
            "{",
            "  if (!{0}_check (self, isolate))".format(component.wrapper_function_prefix),
            "    return;",
        ])

        if len(args) > 0:
            lines.append("")

            for arg in args:
                type_raw = arg.type_raw_for_cpp()
                if type_raw == "$array":
                    type_raw = "Local<Array>"
                lines.append("  {0} {1};".format(type_raw, arg.name_raw_for_cpp()))

            arglist_signature = "".join([arg.type_format_for_cpp() for arg in args])
            arglist_pointers = ", ".join(["&" + arg.name_raw_for_cpp() for arg in args])

            lines.extend([
                "  if (!_gum_v8_args_parse (args, \"{0}\", {1}))".format(arglist_signature, arglist_pointers),
                "    return;",
            ])

        args_needing_conversion = [arg for arg in args if arg.type_converter_for_cpp() is not None]
        if len(args_needing_conversion) > 0:
            lines.append("")
            for arg in args_needing_conversion:
                converter = arg.type_converter_for_cpp()
                if converter == "label":
                    lines.append("  auto {value} = {wrapper_function_prefix}_resolve_label (self, {value_raw});".format(
                        value=arg.name,
                        value_raw=arg.name_raw_for_cpp(),
                        wrapper_function_prefix=component.wrapper_function_prefix))
                elif converter == "address":
                    lines.append("  auto {value} = GUM_ADDRESS ({value_raw});".format(
                        value=arg.name,
                        value_raw=arg.name_raw_for_cpp()))
                elif converter == "bytes":
                    lines.extend([
                        "  gsize {0}_size;".format(arg.name),
                        "  auto {value} = (const guint8 *) g_bytes_get_data ({value_raw}, &{value}_size);".format(
                            value=arg.name,
                            value_raw=arg.name_raw_for_cpp()),
                    ])
                else:
                    lines.extend([
                        "  {0} {1};".format(arg.type, arg.name),
                        "  if (!_gum_parse_{arch}_{type} (isolate, {value_raw}, &{value}))".format(
                            value=arg.name,
                            value_raw=arg.name_raw_for_cpp(),
                            arch=component.arch,
                            type=arg.type_converter_for_cpp()),
                        "    return;",
                    ])

        if is_put_array:
            lines.extend(generate_v8_parse_array_elements(array_item_type, array_item_parse_logic).split("\n"))

        impl_function_name = "{0}_{1}".format(component.impl_function_prefix, method.name)

        arglist = ["self->impl"]
        if method.needs_calling_convention_arg:
            arglist.append("GUM_CALL_CAPI")
        for arg in args:
            if arg.type_converter_for_cpp() == "bytes":
                arglist.extend([arg.name, arg.name + "_size"])
            else:
                arglist.append(arg.name)
        if is_put_array:
            impl_function_name += "_array"
            arglist.insert(len(arglist) - 1, "items_length")

        if method.return_type == "void":
            return_capture = ""
        else:
            return_capture = "auto result = "

        lines.extend([
            "",
            "  {0}{1} ({2});".format(return_capture, impl_function_name, ", ".join(arglist))
        ])

        if method.return_type == "gboolean" and method.name.startswith("put_"):
            lines.extend([
                "  if (!result)",
                "    _gum_v8_throw_ascii_literal (isolate, \"invalid argument\");",
            ])
        elif method.return_type != "void":
            lines.append("")
            if method.return_type == "gboolean":
                lines.append("  info.GetReturnValue ().Set (!!result);")
            elif method.return_type == "guint":
                lines.append("  info.GetReturnValue ().Set ((uint32_t) result);")
            elif method.return_type == "gpointer":
                lines.append("  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (result, core));")
            elif method.return_type == "GumAddress":
                lines.append("  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (GSIZE_TO_POINTER (result), core));")
            elif method.return_type == "cs_insn *":
                target = "\n".join([
                    "self->impl->input_start + (result->address -",
                    "          (self->impl->input_pc -",
                    "            (self->impl->input_cur - self->impl->input_start)))",
                ])
                if component.flavor == "thumb":
                    target = "GSIZE_TO_POINTER (GPOINTER_TO_SIZE ({0}) | 1)".format(target)
                lines.extend([
                    "  if (result != NULL)",
                    "  {",
                    "    info.GetReturnValue ().Set (_gum_v8_instruction_new (self->impl->capstone, result, FALSE,",
                    "        {0}, module->instruction));".format(target),
                    "  }",
                    "  else",
                    "  {",
                    "    info.GetReturnValue ().SetNull ();"
                    "  }",
                ])
            else:
                raise ValueError("Unsupported return type: {0}".format(method.return_type))

        args_needing_cleanup = [arg for arg in args if arg.type_converter_for_cpp() == "bytes"]
        if len(args_needing_cleanup) > 0:
            lines.append("")
            for arg in args_needing_cleanup:
                lines.append("  g_bytes_unref ({0});".format(arg.name_raw_for_cpp()))

        lines.extend([
            "}",
            ""
        ])

    lines.extend([
        "static const GumV8Function {0}_functions[] =".format(component.gumjs_function_prefix),
        "{",
        "  {{ \"reset\", {0}_reset }},".format(component.gumjs_function_prefix),
        "  {{ \"dispose\", {0}_dispose }},".format(component.gumjs_function_prefix),
    ])
    if component.name == "writer":
        lines.append("  {{ \"flush\", {0}_flush }},".format(component.gumjs_function_prefix))
    elif component.name == "relocator":
        lines.append("  {{ \"readOne\", {0}_read_one }},".format(component.gumjs_function_prefix))

    for method in api.instance_methods:
        lines.append("  {{ \"{0}\", {1}_{2} }},".format(
            method.name_js,
            component.gumjs_function_prefix,
            method.name
        ))

    lines.extend([
        "",
        "  { NULL, NULL }",
        "};",
        ""
    ])

    lines.extend(conversion_code)

    return "\n".join(lines)

def generate_v8_parse_array_elements(item_type, parse_item):
    return """
  auto context = isolate->GetCurrentContext ();

  uint32_t items_length = items_value->Length ();
  auto items = g_newa ({item_type}, items_length);

  for (uint32_t items_index = 0; items_index != items_length; items_index++)
  {{
    {item_type} * item = &items[items_index];
{parse_item}
  }}""".format(item_type=item_type, parse_item=parse_item)

def generate_v8_parse_call_arg_array_element(component, api):
    return """
    auto value = items_value->Get (context, items_index).ToLocalChecked ();
    if (value->IsString ())
    {{
      item->type = GUM_ARG_REGISTER;

      String::Utf8Value value_as_utf8 (isolate, value);
      {native_register_type} value_as_reg;
      if (!_gum_parse_{arch}_register (isolate, *value_as_utf8, &value_as_reg))
        return;
      item->value.reg = value_as_reg;
    }}
    else
    {{
      item->type = GUM_ARG_ADDRESS;

      gpointer ptr;
      if (!_gum_v8_native_pointer_parse (value, &ptr, core))
        return;
      item->value.address = GUM_ADDRESS (ptr);
    }}""".format(arch=component.arch, native_register_type=api.native_register_type)

def generate_v8_parse_register_array_element(component, api):
    return """
    auto value = items_value->Get (context, items_index).ToLocalChecked ();
    if (!value->IsString ())
    {{
      _gum_v8_throw_ascii_literal (isolate, "expected an array with register names");
      return;
    }}

    String::Utf8Value value_as_utf8 (isolate, value);
    {native_register_type} value_as_reg;
    if (!_gum_parse_{arch}_register (isolate, *value_as_utf8, &value_as_reg))
      return;

    *item = value_as_reg;""".format(arch=component.arch, native_register_type=api.native_register_type)

def generate_v8_fields(component):
    return """\
  GHashTable * {flavor}_{name}s;
  v8::Global<v8::FunctionTemplate> * {flavor}_{name};""".format(**component.__dict__)

def generate_v8_methods(component):
    params = dict(component.__dict__)

    if component.name == "writer":
        extra_fields = "\n  GHashTable * labels;"
    elif component.name == "relocator":
        extra_fields = "\n  GumV8InstructionValue * input;"

    params["extra_fields"] = extra_fields

    template = """\
#include <gum/arch-{arch}/gum{flavor}{name}.h>

struct {wrapper_struct_name}
{{
  v8::Global<v8::Object> * object;
  {impl_struct_name} * impl;{extra_fields}
  {module_struct_name} * module;
}};

G_GNUC_INTERNAL gboolean _gum_v8_{flavor}_writer_get (v8::Local<v8::Value> value,
    {impl_struct_name} ** writer, {module_struct_name} * module);

G_GNUC_INTERNAL {wrapper_struct_name} * _{wrapper_function_prefix}_new_persistent ({module_struct_name} * module);
G_GNUC_INTERNAL void _{wrapper_function_prefix}_release_persistent ({wrapper_struct_name} * {name});
G_GNUC_INTERNAL void _{wrapper_function_prefix}_init ({wrapper_struct_name} * self, {module_struct_name} * module);
G_GNUC_INTERNAL void _{wrapper_function_prefix}_finalize ({wrapper_struct_name} * self);
G_GNUC_INTERNAL void _{wrapper_function_prefix}_reset ({wrapper_struct_name} * self, {impl_struct_name} * impl);"""
    return template.format(**params) + generate_v8_enum_parser_declarations(component)

def generate_v8_init_code(component):
    return """\
  auto {flavor}_{name} = _gum_v8_create_class ("{gumjs_class_name}",
      {gumjs_function_prefix}_construct, scope, module, isolate);
  _gum_v8_class_add ({flavor}_{name}, {gumjs_function_prefix}_values, module,
      isolate);
  _gum_v8_class_add ({flavor}_{name}, {gumjs_function_prefix}_functions, module,
      isolate);
  self->{flavor}_{name} =
      new Global<FunctionTemplate> (isolate, {flavor}_{name});

  self->{flavor}_{name}s = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) {wrapper_function_prefix}_free);
""".format(**component.__dict__)

def generate_v8_dispose_code(component):
    return """\
  g_hash_table_unref (self->{flavor}_{name}s);
  self->{flavor}_{name}s = NULL;

  delete self->{flavor}_{name};
  self->{flavor}_{name} = nullptr;
""".format(**component.__dict__)

def generate_v8_base_methods(component):
    if component.name == "writer":
        return generate_v8_writer_base_methods(component)
    elif component.name == "relocator":
        return generate_v8_relocator_base_methods(component)

def generate_v8_writer_base_methods(component):
    template = """\
static {wrapper_struct_name} * {wrapper_function_prefix}_alloc (GumV8CodeWriter * module);
static void {wrapper_function_prefix}_dispose ({wrapper_struct_name} * self);
static void {wrapper_function_prefix}_mark_weak ({wrapper_struct_name} * self);
static void {wrapper_function_prefix}_on_weak_notify (
    const WeakCallbackInfo<{wrapper_struct_name}> & info);
static gboolean {gumjs_function_prefix}_parse_constructor_args (const GumV8Args * args,
    gpointer * code_address, GumAddress * pc, gboolean * pc_specified);
static gboolean {wrapper_function_prefix}_check ({wrapper_struct_name} * self,
    Isolate * isolate);

gboolean
_gum_v8_{flavor}_writer_get (
    v8::Local<v8::Value> value,
    {impl_struct_name} ** writer,
    {module_struct_name} * module)
{{
  auto isolate = module->core->isolate;

  auto writer_class = Local<FunctionTemplate>::New (isolate,
      *module->{flavor}_writer);
  if (!writer_class->HasInstance (value))
  {{
    _gum_v8_throw_ascii_literal (isolate, "expected {flavor} writer");
    return FALSE;
  }}

  auto wrapper = ({wrapper_struct_name} *)
      value.As<Object> ()->GetAlignedPointerFromInternalField (0);
  if (!{wrapper_function_prefix}_check (wrapper, isolate))
    return FALSE;

  *writer = wrapper->impl;
  return TRUE;
}}

{wrapper_struct_name} *
_{wrapper_function_prefix}_new_persistent (GumV8CodeWriter * module)
{{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto writer = {wrapper_function_prefix}_alloc (module);

  auto writer_class = Local<FunctionTemplate>::New (isolate,
      *module->{flavor}_writer);

  auto writer_value = External::New (isolate, writer);
  Local<Value> argv[] = {{ writer_value }};

  auto object = writer_class->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, G_N_ELEMENTS (argv), argv).ToLocalChecked ();

  writer->object = new Global<Object> (isolate, object);

  return writer;
}}

void
_{wrapper_function_prefix}_release_persistent ({wrapper_struct_name} * writer)
{{
  {wrapper_function_prefix}_dispose (writer);

  {wrapper_function_prefix}_mark_weak (writer);
}}

void
_{wrapper_function_prefix}_init (
    {wrapper_struct_name} * self,
    {module_struct_name} * module)
{{
  self->object = nullptr;
  self->impl = NULL;
  self->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->module = module;
}}

void
_{wrapper_function_prefix}_finalize ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);

  g_hash_table_unref (self->labels);

  delete self->object;
}}

void
_{wrapper_function_prefix}_reset (
    {wrapper_struct_name} * self,
    {impl_struct_name} * impl)
{{
  if (impl != NULL)
    {impl_function_prefix}_ref (impl);
  if (self->impl != NULL)
    {impl_function_prefix}_unref (self->impl);
  self->impl = impl;

  g_hash_table_remove_all (self->labels);
}}

static {wrapper_struct_name} *
{wrapper_function_prefix}_alloc (GumV8CodeWriter * module)
{{
  {wrapper_struct_name} * writer;

  writer = g_slice_new ({wrapper_struct_name});
  _{wrapper_function_prefix}_init (writer, module);

  return writer;
}}

static void
{wrapper_function_prefix}_dispose ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);
}}

static void
{wrapper_function_prefix}_free ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_finalize (self);

  g_slice_free ({wrapper_struct_name}, self);
}}

static void
{wrapper_function_prefix}_mark_weak ({wrapper_struct_name} * self)
{{
  self->object->SetWeak (self, {wrapper_function_prefix}_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (self->module->{flavor}_{name}s, self);
}}

{label_resolver}
static void
{wrapper_function_prefix}_on_weak_notify (
    const WeakCallbackInfo<{wrapper_struct_name}> & info)
{{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();

  g_hash_table_remove (self->module->{flavor}_{name}s, self);
}}

static gboolean
{wrapper_function_prefix}_check (
    {wrapper_struct_name} * self,
    Isolate * isolate)
{{
  if (self->impl == NULL)
  {{
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return FALSE;
  }}

  return TRUE;
}}

GUMJS_DEFINE_CONSTRUCTOR ({gumjs_function_prefix}_construct)
{{
  if (!info.IsConstructCall ())
  {{
    _gum_v8_throw_ascii_literal (isolate,
        "use constructor syntax to create a new instance");
    return;
  }}

  {wrapper_struct_name} * writer;

  if (info.Length () == 1 && info[0]->IsExternal ())
  {{
    writer = ({wrapper_struct_name} *) info[0].As<External> ()->Value ();
  }}
  else
  {{
    gpointer code_address;
    GumAddress pc;
    gboolean pc_specified;
    if (!{gumjs_function_prefix}_parse_constructor_args (args, &code_address, &pc,
        &pc_specified))
      return;

    writer = {wrapper_function_prefix}_alloc (module);

    writer->object = new Global<Object> (isolate, wrapper);
    {wrapper_function_prefix}_mark_weak (writer);

    writer->impl = {impl_function_prefix}_new (code_address);
    writer->impl->flush_on_destroy = FALSE;
    if (pc_specified)
      writer->impl->pc = pc;
  }}

  wrapper->SetAlignedPointerInInternalField (0, writer);
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_reset, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  gpointer code_address;
  GumAddress pc;
  gboolean pc_specified;
  if (!{gumjs_function_prefix}_parse_constructor_args (args, &code_address, &pc,
      &pc_specified))
    return;

  {impl_function_prefix}_flush (self->impl);

  {impl_function_prefix}_reset (self->impl, code_address);
  if (pc_specified)
    self->impl->pc = pc;

  g_hash_table_remove_all (self->labels);
}}

static gboolean
{gumjs_function_prefix}_parse_constructor_args (
    const GumV8Args * args,
    gpointer * code_address,
    GumAddress * pc,
    gboolean * pc_specified)
{{
  auto isolate = args->core->isolate;

  Local<Object> options;
  if (!_gum_v8_args_parse (args, "p|O", code_address, &options))
    return FALSE;

  *pc = 0;
  *pc_specified = FALSE;

  if (!options.IsEmpty ())
  {{
    auto context = isolate->GetCurrentContext ();

    Local<Value> pc_value;
    if (!options->Get (context, _gum_v8_string_new_ascii (isolate, "pc"))
        .ToLocal (&pc_value))
    {{
      return FALSE;
    }}

    if (!pc_value->IsUndefined ())
    {{
      gpointer raw_value;
      if (!_gum_v8_native_pointer_get (pc_value, &raw_value, args->core))
        return FALSE;
      *pc = GUM_ADDRESS (raw_value);
      *pc_specified = TRUE;
    }}
  }}

  return TRUE;
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_dispose, {wrapper_struct_name})
{{
  if (self->impl != NULL)
    {impl_function_prefix}_flush (self->impl);

  {wrapper_function_prefix}_dispose (self);
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_flush, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  auto success = {impl_function_prefix}_flush (self->impl);
  if (!success)
    _gum_v8_throw_ascii_literal (isolate, "unable to resolve references");
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_base, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (self->impl->base, core));
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_code, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (self->impl->code, core));
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_pc, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (self->impl->pc), core));
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_offset, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set ({impl_function_prefix}_offset (self->impl));
}}

static const GumV8Property {gumjs_function_prefix}_values[] =
{{
  {{ "base", {gumjs_function_prefix}_get_base, NULL }},
  {{ "code", {gumjs_function_prefix}_get_code, NULL }},
  {{ "pc", {gumjs_function_prefix}_get_pc, NULL }},
  {{ "offset", {gumjs_function_prefix}_get_offset, NULL }},

  {{ NULL, NULL, NULL }}
}};
"""

    params = dict(component.__dict__)

    params["label_resolver"] = """
static gconstpointer
{wrapper_function_prefix}_resolve_label ({wrapper_struct_name} * self,
    const std::string & str)
{{
  gchar * label = (gchar *) g_hash_table_lookup (self->labels, str.c_str ());
  if (label != NULL)
    return label;

  label = g_strdup (str.c_str ());
  g_hash_table_add (self->labels, label);
  return label;
}}
""".format(**params)

    return template.format(**params).split("\n")

def generate_v8_relocator_base_methods(component):
    template = """\
static {wrapper_struct_name} * {wrapper_function_prefix}_alloc (GumV8CodeRelocator * module);
static void {wrapper_function_prefix}_dispose ({wrapper_struct_name} * self);
static void {wrapper_function_prefix}_mark_weak ({wrapper_struct_name} * self);
static void {wrapper_function_prefix}_on_weak_notify (
    const WeakCallbackInfo<{wrapper_struct_name}> & info);
static gboolean {wrapper_function_prefix}_check ({wrapper_struct_name} * self,
    Isolate * isolate);
static gboolean {gumjs_function_prefix}_parse_constructor_args (const GumV8Args * args,
    gconstpointer * input_code, {writer_impl_struct_name} ** writer,
    GumV8CodeRelocator * module);

gboolean
_gum_v8_{flavor}_relocator_get (
    v8::Local<v8::Value> value,
    {impl_struct_name} ** relocator,
    {module_struct_name} * module)
{{
  auto isolate = module->core->isolate;

  auto relocator_class = Local<FunctionTemplate>::New (isolate,
      *module->{flavor}_relocator);
  if (!relocator_class->HasInstance (value))
  {{
    _gum_v8_throw_ascii_literal (isolate, "expected {flavor} relocator");
    return FALSE;
  }}

  auto relocator_wrapper = ({wrapper_struct_name} *)
      value.As<Object> ()->GetAlignedPointerFromInternalField (0);
  if (!{wrapper_function_prefix}_check (relocator_wrapper, isolate))
    return FALSE;

  *relocator = relocator_wrapper->impl;
  return TRUE;
}}

{wrapper_struct_name} *
_{wrapper_function_prefix}_new_persistent (GumV8CodeRelocator * module)
{{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto relocator = {wrapper_function_prefix}_alloc (module);

  auto relocator_class = Local<FunctionTemplate>::New (isolate,
      *module->{flavor}_relocator);

  auto relocator_value = External::New (isolate, relocator);
  Local<Value> argv[] = {{ relocator_value }};

  auto object = relocator_class->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, G_N_ELEMENTS (argv), argv).ToLocalChecked ();

  relocator->object = new Global<Object> (isolate, object);

  return relocator;
}}

void
_{wrapper_function_prefix}_release_persistent ({wrapper_struct_name} * relocator)
{{
  {wrapper_function_prefix}_dispose (relocator);

  {wrapper_function_prefix}_mark_weak (relocator);
}}

void
_{wrapper_function_prefix}_init (
    {wrapper_struct_name} * self,
    {module_struct_name} * module)
{{
  self->object = nullptr;
  self->impl = NULL;
  self->input = _gum_v8_instruction_new_persistent (module->instruction);
  self->module = module;
}}

void
_{wrapper_function_prefix}_finalize ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);

  _gum_v8_instruction_release_persistent (self->input);

  delete self->object;
}}

void
_{wrapper_function_prefix}_reset (
    {wrapper_struct_name} * self,
    {impl_struct_name} * impl)
{{
  if (impl != NULL)
    {impl_function_prefix}_ref (impl);
  if (self->impl != NULL)
    {impl_function_prefix}_unref (self->impl);
  self->impl = impl;

  self->input->insn = NULL;
}}

static {wrapper_struct_name} *
{wrapper_function_prefix}_alloc (GumV8CodeRelocator * module)
{{
  {wrapper_struct_name} * relocator;

  relocator = g_slice_new ({wrapper_struct_name});
  _{wrapper_function_prefix}_init (relocator, module);

  return relocator;
}}

static void
{wrapper_function_prefix}_dispose ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_reset (self, NULL);
}}

static void
{wrapper_function_prefix}_free ({wrapper_struct_name} * self)
{{
  _{wrapper_function_prefix}_finalize (self);

  g_slice_free ({wrapper_struct_name}, self);
}}

static void
{wrapper_function_prefix}_mark_weak ({wrapper_struct_name} * self)
{{
  self->object->SetWeak (self, {wrapper_function_prefix}_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (self->module->{flavor}_{name}s, self);
}}

static void
{wrapper_function_prefix}_on_weak_notify (
    const WeakCallbackInfo<{wrapper_struct_name}> & info)
{{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();

  g_hash_table_remove (self->module->{flavor}_{name}s, self);
}}

static gboolean
{wrapper_function_prefix}_check (
    {wrapper_struct_name} * self,
    Isolate * isolate)
{{
  if (self->impl == NULL)
  {{
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return FALSE;
  }}

  return TRUE;
}}

GUMJS_DEFINE_CONSTRUCTOR ({gumjs_function_prefix}_construct)
{{
  if (!info.IsConstructCall ())
  {{
    _gum_v8_throw_ascii_literal (isolate,
        "use constructor syntax to create a new instance");
    return;
  }}

  {wrapper_struct_name} * relocator;

  if (info.Length () == 1 && info[0]->IsExternal ())
  {{
    relocator = ({wrapper_struct_name} *) info[0].As<External> ()->Value ();
  }}
  else
  {{
    gconstpointer input_code;
    {writer_impl_struct_name} * writer;
    if (!{gumjs_function_prefix}_parse_constructor_args (args, &input_code, &writer, module))
      return;

    relocator = {wrapper_function_prefix}_alloc (module);

    relocator->object = new Global<Object> (isolate, wrapper);
    {wrapper_function_prefix}_mark_weak (relocator);

    relocator->impl = {impl_function_prefix}_new (input_code, writer);
  }}

  wrapper->SetAlignedPointerInInternalField (0, relocator);
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_reset, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  gconstpointer input_code;
  {writer_impl_struct_name} * writer;
  if (!{gumjs_function_prefix}_parse_constructor_args (args, &input_code, &writer, module))
    return;

  {impl_function_prefix}_reset (self->impl, input_code, writer);

  self->input->insn = NULL;
}}

static gboolean
{gumjs_function_prefix}_parse_constructor_args (
    const GumV8Args * args,
    gconstpointer * input_code,
    {writer_impl_struct_name} ** writer,
    {module_struct_name} * module)
{{
  Local<Object> writer_object;
  if (!_gum_v8_args_parse (args, "pO", input_code, &writer_object))
    return FALSE;

  if (!_gum_v8_{flavor}_writer_get (writer_object, writer, module->writer))
    return FALSE;

  return TRUE;
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_dispose, {wrapper_struct_name})
{{
  {wrapper_function_prefix}_dispose (self);
}}

GUMJS_DEFINE_CLASS_METHOD ({gumjs_function_prefix}_read_one, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  uint32_t n_read = {impl_function_prefix}_read_one (self->impl, &self->input->insn);
  if (n_read != 0)
  {{
    self->input->target = {get_input_target_expression};
  }}

  info.GetReturnValue ().Set (n_read);
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_input, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  if (self->input->insn != NULL)
  {{
    info.GetReturnValue ().Set (
        Local<Object>::New (isolate, *self->input->object));
  }}
  else
  {{
    info.GetReturnValue ().SetNull ();
  }}
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_eob, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set (!!{impl_function_prefix}_eob (self->impl));
}}

GUMJS_DEFINE_CLASS_GETTER ({gumjs_function_prefix}_get_eoi, {wrapper_struct_name})
{{
  if (!{wrapper_function_prefix}_check (self, isolate))
    return;

  info.GetReturnValue ().Set (!!{impl_function_prefix}_eoi (self->impl));
}}

static const GumV8Property {gumjs_function_prefix}_values[] =
{{
  {{ "input", {gumjs_function_prefix}_get_input, NULL }},
  {{ "eob", {gumjs_function_prefix}_get_eob, NULL }},
  {{ "eoi", {gumjs_function_prefix}_get_eoi, NULL }},

  {{ NULL, NULL, NULL }}
}};
"""

    target = "self->impl->input_cur - self->input->insn->size"
    if component.flavor == "thumb":
        target = "GSIZE_TO_POINTER (GPOINTER_TO_SIZE ({0}) | 1)".format(target)

    params = {
        "writer_impl_struct_name": to_camel_case('gum_{0}_writer'.format(component.flavor), start_high=True),
        "get_input_target_expression": target,
    }
    params.update(component.__dict__)

    return template.format(**params).split("\n")

def generate_v8_enum_parser(name, type, prefix, values):
    common_decls, common_code = generate_enum_parser(name, type, prefix, values)

    params = {
        'name': name,
        'description': name.replace("_", " "),
        'type': type,
    }

    decls = common_decls

    code = """\
gboolean
_gum_parse_{name} (
    Isolate * isolate,
    const std::string & name,
    {type} * value)
{{
  if (!gum_try_parse_{name} (name.c_str (), value))
  {{
    _gum_v8_throw_literal (isolate, "invalid {description}");
    return FALSE;
  }}

  return TRUE;
}}
""".format(**params).split("\n") + common_code

    return (decls, code)

arch_names = {
    "x86": "x86",
    "arm": "ARM",
    "arm64": "AArch64",
    "mips": "MIPS",
}

writer_enums = {
    "x86": [
        ("x86_register", "GumX86Reg", "GUM_X86_", [
            "xax", "xcx", "xdx", "xbx", "xsp", "xbp", "xsi", "xdi",
            "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
            "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
            "xip", "eip", "rip",
        ]),
        ("x86_instruction_id", "x86_insn", "X86_INS_", [
            "jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja", "js", "jns",
            "jp", "jnp", "jl", "jge", "jle", "jg", "jcxz", "jecxz", "jrcxz",
        ]),
        ("x86_branch_hint", "GumBranchHint", "GUM_", [
            "no-hint", "likely", "unlikely",
        ]),
        ("x86_pointer_target", "GumX86PtrTarget", "GUM_X86_PTR_", [
            "byte", "dword", "qword",
        ]),
    ],
    "arm": [
        ("arm_register", "arm_reg", "ARM_REG_", [
            "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "sp", "lr", "sb", "sl", "fp", "ip", "pc",
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9",
            "s10", "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19",
            "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29",
            "s30", "s31",
            "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
            "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
            "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29",
            "d30", "d31",
            "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9",
            "q10", "q11", "q12", "q13", "q14", "q15",
        ]),
        ("arm_system_register", "arm_sysreg", "ARM_SYSREG_", [
            "apsr-nzcvq",
        ]),
        ("arm_condition_code", "arm_cc", "ARM_CC_", [
            "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
            "hi", "ls", "ge", "lt", "gt", "le", "al",
        ]),
        ("arm_shifter", "arm_shifter", "ARM_SFT_", [
            "asr", "lsl", "lsr", "ror", "rrx", "asr-reg", "lsl-reg", "lsr-reg",
            "ror-reg", "rrx-reg",
        ]),
    ],
    "thumb": [],
    "arm64": [
        ("arm64_register", "arm64_reg", "ARM64_REG_", [
            "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9",
            "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19",
            "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29",
            "x30",
            "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9",
            "w10", "w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19",
            "w20", "w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29",
            "w30",
            "sp", "lr", "fp",
            "wsp", "wzr", "xzr", "nzcv", "ip0", "ip1",
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9",
            "s10", "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19",
            "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29",
            "s30", "s31",
            "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
            "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
            "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29",
            "d30", "d31",
            "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9",
            "q10", "q11", "q12", "q13", "q14", "q15", "q16", "q17", "q18", "q19",
            "q20", "q21", "q22", "q23", "q24", "q25", "q26", "q27", "q28", "q29",
            "q30", "q31",
        ]),
        ("arm64_condition_code", "arm64_cc", "ARM64_CC_", [
            "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
            "hi", "ls", "ge", "lt", "gt", "le", "al", "nv",
        ]),
        ("arm64_index_mode", "GumArm64IndexMode", "GUM_INDEX_", [
            "post-adjust", "signed-offset", "pre-adjust",
        ]),
    ],
    "mips": [
        ("mips_register", "mips_reg", "MIPS_REG_", [
            "v0", "v1", "a0", "a1", "a2", "a3",
            "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
            "t8", "t9",
            "k0", "k1",
            "gp", "sp", "fp", "s8", "ra",
            "hi", "lo", "zero", "at",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
            "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
            "30", "31",
        ]),
    ],
}

def generate_conversion_methods(component, generate_parser):
    decls = []
    code = []

    if component.name == "writer":
        for enum in writer_enums[component.flavor]:
            d, c = generate_parser(*enum)
            decls += d
            code += c

    return (decls, code)

def generate_quick_enum_parser_declarations(component):
    if component.name != "writer":
        return ""
    return "".join(
        "\nG_GNUC_INTERNAL gboolean _gum_parse_{name} (JSContext * ctx, const gchar * name, {type} * value);".format(name=name, type=type)
        for name, type, prefix, values in writer_enums[component.flavor]
    )

def generate_v8_enum_parser_declarations(component):
    if component.name != "writer":
        return ""
    return "".join(
        "\nG_GNUC_INTERNAL gboolean _gum_parse_{name} (v8::Isolate * isolate, const std::string & name, {type} * value);".format(name=name, type=type)
        for name, type, prefix, values in writer_enums[component.flavor]
    )

def generate_enum_parser(name, type, prefix, values):
    decls = [
        "static gboolean gum_try_parse_{name} (const gchar * name, {type} * value);".format(name=name, type=type)
    ]

    statements = []
    for i, value in enumerate(values):
        statements.extend([
            f"  if (strcmp (name, \"{value}\") == 0)",
            "  {",
            "    *value = {}{};".format(prefix, value.upper().replace("-", "_")),
            "    return TRUE;",
            "  }",
        ])

    code = """\
static gboolean
gum_try_parse_{name} (
    const gchar * name,
    {type} * value)
{{
{statements}

  return FALSE;
}}
""".format(
        name=name,
        type=type,
        statements="\n".join(statements),
    )

    return (decls, code.split("\n"))

def generate_tsds(name, arch, flavor, api):
    tsds = {}
    tsds.update(generate_class_type_definitions(name, arch, flavor, api))
    tsds.update(generate_enum_type_definitions(name, arch, flavor, api))
    return tsds

def generate_class_type_definitions(name, arch, flavor, api):
    lines = []

    class_name = to_camel_case("{0}_{1}".format(flavor, name), start_high=True)
    writer_class_name = to_camel_case("{0}_writer".format(flavor, "writer"), start_high=True)

    params = {
        "arch": arch,
        "arch_name": arch_names[arch],
        "arch_namespace": arch.title(),
        "class_name": class_name,
        "writer_class_name": writer_class_name,
    }

    if name == "writer":
        class_description = "Generates machine code for {0}.".format(arch)
    else:
        class_description = "Relocates machine code for {0}.".format(arch)

    lines.extend([
        "/**",
        " * " + class_description,
        " */",
        "declare class {0} {{".format(class_name),
    ])

    if name == "writer":
        lines.extend("""\
    /**
     * Creates a new code writer for generating {arch_name} machine code
     * written directly to memory at `codeAddress`.
     *
     * @param codeAddress Memory address to write generated code to.
     * @param options Options for customizing code generation.
     */
    constructor(codeAddress: NativePointerValue, options?: {class_name}Options);

    /**
     * Recycles instance.
     */
    reset(codeAddress: NativePointerValue, options?: {class_name}Options): void;

    /**
     * Eagerly cleans up memory.
     */
    dispose(): void;

    /**
     * Resolves label references and writes pending data to memory. You
     * should always call this once you've finished generating code. It
     * is usually also desirable to do this between pieces of unrelated
     * code, e.g. when generating multiple functions in one go.
     */
    flush(): void;

    /**
     * Memory location of the first byte of output.
     */
    base: NativePointer;

    /**
     * Memory location of the next byte of output.
     */
    code: NativePointer;

    /**
     * Program counter at the next byte of output.
     */
    pc: NativePointer;

    /**
     * Current offset in bytes.
     */
    offset: number;""".format(**params).split("\n"))
    elif name == "relocator":
        lines.extend("""\
    /**
     * Creates a new code relocator for copying {arch_name} instructions
     * from one memory location to another, taking care to adjust
     * position-dependent instructions accordingly.
     *
     * @param inputCode Source address to copy instructions from.
     * @param output {writer_class_name} pointed at the desired target memory
     *               address.
     */
    constructor(inputCode: NativePointerValue, output: {writer_class_name});

    /**
     * Recycles instance.
     */
    reset(inputCode: NativePointerValue, output: {writer_class_name}): void;

    /**
     * Eagerly cleans up memory.
     */
    dispose(): void;

    /**
     * Latest `Instruction` read so far. Starts out `null` and changes
     * on every call to `readOne()`.
     */
    input: Instruction | null;

    /**
     * Indicates whether end-of-block has been reached, i.e. we've
     * reached a branch of any kind, like CALL, JMP, BL, RET.
     */
    eob: boolean;

    /**
     * Indicates whether end-of-input has been reached, e.g. we've
     * reached JMP/B/RET, an instruction after which there may or may
     * not be valid code.
     */
    eoi: boolean;

    /**
     * Reads the next instruction into the relocator's internal buffer
     * and returns the number of bytes read so far, including previous
     * calls.
     *
     * You may keep calling this method to keep buffering, or immediately
     * call either `writeOne()` or `skipOne()`. Or, you can buffer up
     * until the desired point and then call `writeAll()`.
     *
     * Returns zero when end-of-input is reached, which means the `eoi`
     * property is now `true`.
     */
    readOne(): number;""".format(**params).split("\n"))

    for method in api.instance_methods:
        arg_names = [arg.name_js for arg in method.args]

        description = ""
        if method.name.startswith("put_"):
            if method.name == "put_label":
                description = """Puts a label at the current position, where `id` is an identifier
     * that may be referenced in past and future `put*Label()` calls"""
            elif method.name.startswith("put_call") and "_with_arguments" in method.name:
                description = """Puts code needed for calling a C function with the specified `args`"""
                arg_names[-1] = "args"
            elif method.name.startswith("put_call") and "_with_aligned_arguments" in method.name:
                description = """Like `putCallWithArguments()`, but also
     * ensures that the argument list is aligned on a 16 byte boundary"""
                arg_names[-1] = "args"
            elif method.name == "put_branch_address":
                description = "Puts code needed for branching/jumping to the given address"
            elif method.name in ("put_push_regs", "put_pop_regs"):
                if method.name.startswith("put_push_"):
                    mnemonic = "PUSH"
                else:
                    mnemonic = "POP"
                description = """Puts a {mnemonic} instruction with the specified registers""".format(mnemonic=mnemonic)
                arg_names[-1] = "regs"
            elif method.name == "put_push_all_x_registers":
                description = """Puts code needed for pushing all X registers on the stack"""
            elif method.name == "put_push_all_q_registers":
                description = """Puts code needed for pushing all Q registers on the stack"""
            elif method.name == "put_pop_all_x_registers":
                description = """Puts code needed for popping all X registers off the stack"""
            elif method.name == "put_pop_all_q_registers":
                description = """Puts code needed for popping all Q registers off the stack"""
            elif method.name == "put_prologue_trampoline":
                description = "Puts a minimal sized trampoline for vectoring to the given address"
            elif method.name == "put_ldr_reg_ref":
                description = """Puts an LDR instruction with a dangling data reference,
     * returning an opaque ref value that should be passed to `putLdrRegValue()`
     * at the desired location"""
            elif method.name == "put_ldr_reg_value":
                description = """Puts the value and updates the LDR instruction
     * from a previous `putLdrRegRef()`"""
            elif method.name == "put_breakpoint":
                description = "Puts an OS/architecture-specific breakpoint instruction"
            elif method.name == "put_padding":
                description = "Puts `n` guard instruction"
            elif method.name == "put_nop_padding":
                description = "Puts `n` NOP instructions"
            elif method.name == "put_instruction":
                description = "Puts a raw instruction"
            elif method.name == "put_instruction_wide":
                description = "Puts a raw Thumb-2 instruction"
            elif method.name == "put_u8":
                description = "Puts a uint8"
            elif method.name == "put_s8":
                description = "Puts an int8"
            elif method.name == "put_bytes":
                description = "Puts raw data"
            elif method.name.endswith("no_auth"):
                opcode = method.name.split("_")[1].upper()
                description = """Puts {0} instruction expecting a raw pointer without
     * any authentication bits""".format(make_indefinite(opcode))
            else:
                types = set(["reg", "imm", "offset", "indirect", "short", "near", "ptr", "base", "index", "scale", "address", "label", "u8", "i32", "u32", "u64"])
                opcode = " ".join(filter(lambda token: token not in types, method.name.split("_")[1:])).upper()
                description = "Puts {0} instruction".format(make_indefinite(opcode))
                if method.name.endswith("_label"):
                    description += """ referencing `labelId`, defined by a past
     * or future `putLabel()`"""
        elif method.name == "skip":
            description = "Skips `nBytes`"
        elif method.name == "peek_next_write_insn":
            description = "Peeks at the next `Instruction` to be written or skipped".format(**params)
        elif method.name == "peek_next_write_source":
            description = "Peeks at the address of the next instruction to be written or skipped"
        elif method.name.startswith("skip_one"):
            description = "Skips the instruction that would have been written next"
            if method.name.endswith("_no_label"):
                description += """,
     * but without a label for internal use. This breaks relocation of branches to
     * locations inside the relocated range, and is an optimization for use-cases
     * where all branches are rewritten (e.g. Frida's Stalker)"""
        elif method.name.startswith("write_one"):
            description = "Writes the next buffered instruction"
            if method.name.endswith("_no_label"):
                description += """, but without a
     * label for internal use. This breaks relocation of branches to locations
     * inside the relocated range, and is an optimization for use-cases where all
     * branches are rewritten (e.g. Frida's Stalker)"""
        elif method.name == "copy_one":
            description = """Copies out the next buffered instruction without advancing the
     * output cursor, allowing the same instruction to be written out
     * multiple times"""
        elif method.name.startswith("write_all"):
            description = "Writes all buffered instructions"
        elif method.name == "can_branch_directly_between":
            description = """Determines whether a direct branch is possible between the two
     * given memory locations"""
        elif method.name == "commit_label":
            description = """Commits the first pending reference to the given label, returning
     * `true` on success. Returns `false` if the given label hasn't been
     * defined yet, or there are no more pending references to it"""
        elif method.name == "sign":
            description = "Signs the given pointer value"

        p = {}
        p.update(params)
        p.update({
            "method_name": method.name_js,
            "method_arglist": ", ".join([n + ": " + t for n, t in zip(arg_names, [arg.type_ts for arg in method.args])]),
            "method_return_type": method.return_type_ts,
            "method_description": description,
        })

        lines.extend("""\

    /**
     * {method_description}.
     */
    {method_name}({method_arglist}): {method_return_type};""".format(**p).split("\n"))

    lines.append("}")

    if name == "writer":
        lines.extend("""
interface {class_name}Options {{
    /**
     * Specifies the initial program counter, which is useful when
     * generating code to a scratch buffer. This is essential when using
     * `Memory.patchCode()` on iOS, which may provide you with a
     * temporary location that later gets mapped into memory at the
     * intended memory location.
     */
    pc?: NativePointer | undefined;
}}""".format(**params).split("\n"))

        if flavor != "thumb":
            lines.extend([
                "",
                "type {arch_namespace}CallArgument = {arch_namespace}Register | number | UInt64 | Int64 | NativePointerValue;".format(**params),
            ])

    return {
        "{0}-{1}.d.ts".format(flavor, name): "\n".join(lines),
    }

def generate_enum_type_definitions(name, arch, flavor, api):
    lines = []

    for name, type, prefix, values in writer_enums[arch]:
        name_ts = to_camel_case(name, start_high=True)
        name_components = name.replace("_", " ").title().split(" ")

        if len(lines) > 0:
            lines.append("")

        values_ts = " | ".join(["\"{0}\"".format(val) for val in values])
        raw_decl = "type {0} = {1};".format(name_ts, values_ts)
        lines.extend(reflow_enum_declaration(raw_decl))

    return {
        "{0}-enums.d.ts".format(arch): "\n".join(lines),
    }

def reflow_enum_declaration(decl):
    if len(decl.split(" | ")) <= 3:
        return [decl]

    first_line, rest = decl.split(" = ", 1)

    values = rest.rstrip(";").split(" | ")

    return [first_line + " ="] + ["    | {0}".format(val) for val in values] + ["    ;"]

def generate_docs(name, arch, flavor, api):
    docs = {}
    docs.update(generate_class_api_reference(name, arch, flavor, api))
    docs.update(generate_enum_api_reference(name, arch, flavor, api))
    return docs

def generate_class_api_reference(name, arch, flavor, api):
    lines = []

    class_name = to_camel_case("{0}_{1}".format(flavor, name), start_high=True)
    writer_class_name = to_camel_case("{0}_writer".format(flavor, "writer"), start_high=True)

    params = {
        "arch": arch,
        "arch_name": arch_names[arch],
        "class_name": class_name,
        "writer_class_name": writer_class_name,
        "writer_class_link_indefinite": "{0} [{1}](#{2})".format(
            make_indefinite_qualifier(writer_class_name),
            writer_class_name,
            writer_class_name.lower()),
        "instruction_link": "[Instruction](#instruction)",
    }

    lines.extend([
        "## {0}".format(class_name),
        "",
    ])

    if name == "writer":
        lines.extend("""\
+   `new {class_name}(codeAddress[, {{ pc: ptr('0x1234') }}])`: create a new code
    writer for generating {arch_name} machine code written directly to memory at
    `codeAddress`, specified as a NativePointer.
    The second argument is an optional options object where the initial program
    counter may be specified, which is useful when generating code to a scratch
    buffer. This is essential when using `Memory.patchCode()` on iOS, which may
    provide you with a temporary location that later gets mapped into memory at
    the intended memory location.

-   `reset(codeAddress[, {{ pc: ptr('0x1234') }}])`: recycle instance

-   `dispose()`: eagerly clean up memory

-   `flush()`: resolve label references and write pending data to memory. You
    should always call this once you've finished generating code. It is usually
    also desirable to do this between pieces of unrelated code, e.g. when
    generating multiple functions in one go.

-   `base`: memory location of the first byte of output, as a NativePointer

-   `code`: memory location of the next byte of output, as a NativePointer

-   `pc`: program counter at the next byte of output, as a NativePointer

-   `offset`: current offset as a JavaScript Number
""".format(**params).split("\n"))
    elif name == "relocator":
        lines.extend("""\
+   `new {class_name}(inputCode, output)`: create a new code relocator for
    copying {arch_name} instructions from one memory location to another, taking
    care to adjust position-dependent instructions accordingly.
    The source address is specified by `inputCode`, a NativePointer.
    The destination is given by `output`, {writer_class_link_indefinite} pointed
    at the desired target memory address.

-   `reset(inputCode, output)`: recycle instance

-   `dispose()`: eagerly clean up memory

-   `input`: latest {instruction_link} read so far. Starts out `null`
    and changes on every call to `readOne()`.

-   `eob`: boolean indicating whether end-of-block has been reached, i.e. we've
    reached a branch of any kind, like CALL, JMP, BL, RET.

-   `eoi`: boolean indicating whether end-of-input has been reached, e.g. we've
    reached JMP/B/RET, an instruction after which there may or may not be valid
    code.

-   `readOne()`: read the next instruction into the relocator's internal buffer
    and return the number of bytes read so far, including previous calls.
    You may keep calling this method to keep buffering, or immediately call
    either `writeOne()` or `skipOne()`. Or, you can buffer up until the desired
    point and then call `writeAll()`.
    Returns zero when end-of-input is reached, which means the `eoi` property is
    now `true`.
""".format(**params).split("\n"))

    for method in api.instance_methods:
        arg_names = [arg.name_js for arg in method.args]

        description = ""
        if method.name.startswith("put_"):
            if method.name == "put_label":
                description = """put a label at the current position, where `id` is a string
    that may be referenced in past and future `put*Label()` calls"""
            elif method.name.startswith("put_call") and "_with_arguments" in method.name:
                description = """put code needed for calling a C
    function with the specified `args`, specified as a JavaScript array where
    each element is either a string specifying the register, or a Number or
    NativePointer specifying the immediate value."""
                arg_names[-1] = "args"
            elif method.name.startswith("put_call") and "_with_aligned_arguments" in method.name:
                description = """like above, but also
    ensures that the argument list is aligned on a 16 byte boundary"""
                arg_names[-1] = "args"
            elif method.name == "put_branch_address":
                description = """put code needed for branching/jumping to the
    given address"""
            elif method.name in ("put_push_regs", "put_pop_regs"):
                if method.name.startswith("put_push_"):
                    mnemonic = "PUSH"
                else:
                    mnemonic = "POP"
                description = """put a {mnemonic} instruction with the specified registers,
    specified as a JavaScript array where each element is a string specifying
    the register name.""".format(mnemonic=mnemonic)
                arg_names[-1] = "regs"
            elif method.name == "put_push_all_x_registers":
                description = """put code needed for pushing all X registers on the stack"""
            elif method.name == "put_push_all_q_registers":
                description = """put code needed for pushing all Q registers on the stack"""
            elif method.name == "put_pop_all_x_registers":
                description = """put code needed for popping all X registers off the stack"""
            elif method.name == "put_pop_all_q_registers":
                description = """put code needed for popping all Q registers off the stack"""
            elif method.name == "put_prologue_trampoline":
                description = """put a minimal sized trampoline for
    vectoring to the given address"""
            elif method.name == "put_ldr_reg_ref":
                description = """put an LDR instruction with a dangling data reference,
    returning an opaque ref value that should be passed to `putLdrRegValue()`
    at the desired location"""
            elif method.name == "put_ldr_reg_value":
                description = """put the value and update the LDR instruction
    from a previous `putLdrRegRef()`"""
            elif method.name == "put_breakpoint":
                description = "put an OS/architecture-specific breakpoint instruction"
            elif method.name == "put_padding":
                description = "put `n` guard instruction"
            elif method.name == "put_nop_padding":
                description = "put `n` NOP instructions"
            elif method.name == "put_instruction":
                description = "put a raw instruction as a JavaScript Number"
            elif method.name == "put_instruction_wide":
                description = "put a raw Thumb-2 instruction from\n    two JavaScript Number values"
            elif method.name == "put_u8":
                description = "put a uint8"
            elif method.name == "put_s8":
                description = "put an int8"
            elif method.name == "put_bytes":
                description = "put raw data from the provided ArrayBuffer"
            elif method.name.endswith("no_auth"):
                opcode = method.name.split("_")[1].upper()
                description = """put {0} instruction expecting a raw pointer
    without any authentication bits""".format(make_indefinite(opcode))
            else:
                types = set(["reg", "imm", "offset", "indirect", "short", "near", "ptr", "base", "index", "scale", "address", "label", "u8", "i32", "u32", "u64"])
                opcode = " ".join(filter(lambda token: token not in types, method.name.split("_")[1:])).upper()
                description = "put {0} instruction".format(make_indefinite(opcode))
                if method.name.endswith("_label"):
                    description += """
    referencing `labelId`, defined by a past or future `putLabel()`"""
        elif method.name == "skip":
            description = "skip `nBytes`"
        elif method.name == "peek_next_write_insn":
            description = "peek at the next {instruction_link} to be\n    written or skipped".format(**params)
        elif method.name == "peek_next_write_source":
            description = "peek at the address of the next instruction to be\n    written or skipped"
        elif method.name.startswith("skip_one"):
            description = "skip the instruction that would have been written next"
            if method.name.endswith("_no_label"):
                description += """,
    but without a label for internal use. This breaks relocation of branches to
    locations inside the relocated range, and is an optimization for use-cases
    where all branches are rewritten (e.g. Frida's Stalker)."""
        elif method.name.startswith("write_one"):
            description = "write the next buffered instruction"
            if method.name.endswith("_no_label"):
                description += """, but without a
    label for internal use. This breaks relocation of branches to locations
    inside the relocated range, and is an optimization for use-cases where all
    branches are rewritten (e.g. Frida's Stalker)."""
        elif method.name == "copy_one":
            description = """copy out the next buffered instruction without advancing the
    output cursor, allowing the same instruction to be written out multiple
    times"""
        elif method.name.startswith("write_all"):
            description = "write all buffered instructions"
        elif method.name == "can_branch_directly_between":
            description = """determine whether a direct branch is
    possible between the two given memory locations"""
        elif method.name == "commit_label":
            description = """commit the first pending reference to the given label,
    returning `true` on success. Returns `false` if the given label hasn't been
    defined yet, or there are no more pending references to it."""
        elif method.name == "sign":
            description = "sign the given pointer value"

        p = {}
        p.update(params)
        p.update({
            "method_name": method.name_js,
            "method_arglist": ", ".join(arg_names),
            "method_description": description,
        })

        lines.extend("""\
-   `{method_name}({method_arglist})`: {method_description}
""".format(**p).split("\n"))

    return {
        "{0}-{1}.md".format(flavor, name): "\n".join(lines),
    }

def generate_enum_api_reference(name, arch, flavor, api):
    lines = [
        "## {0} enum types".format(arch_names[arch]),
        "",
    ]

    for name, type, prefix, values in writer_enums[arch]:
        display_name = to_camel_case("_".join(name.split("_")[1:]), start_high=True)

        lines.extend(reflow_enum_bulletpoint("-   {0}: `{1}`".format(display_name, "` `".join(values))))

    lines.append("")

    return {
        "{0}-enums.md".format(arch): "\n".join(lines),
    }

def reflow_enum_bulletpoint(bulletpoint):
    result = [bulletpoint]

    indent = 3 * " "

    while True:
        last_line = result[-1]
        if len(last_line) < 80:
            break

        cutoff_index = last_line.rindex("` `", 0, 81) + 1
        before = last_line[:cutoff_index]
        after = indent + last_line[cutoff_index:]

        result[-1] = before
        result.append(after)

    return result

def make_indefinite(noun):
    return make_indefinite_qualifier(noun) + " " + noun

def make_indefinite_qualifier(noun):
    noun_lc = noun.lower()

    exceptions = [
        "ld",
        "lf",
        "rd",
        "x",
    ]
    for prefix in exceptions:
        if noun_lc.startswith(prefix):
            return "an"

    return "an" if noun_lc[0] in ("a", "e", "i", "o", "u") else "a"

class Component(object):
    def __init__(self, name, arch, flavor, namespace):
        self.name = name
        self.arch = arch
        self.flavor = flavor
        self.wrapper_struct_name = to_camel_case("gum_{0}_{1}_{2}".format(namespace, flavor, name), start_high=True)
        self.wrapper_function_prefix = "gum_{0}_{1}_{2}".format(namespace, flavor, name)
        self.impl_struct_name = to_camel_case("gum_{0}_{1}".format(flavor, name), start_high=True)
        self.impl_function_prefix = "gum_{0}_{1}".format(flavor, name)
        self.gumjs_class_name = flavor.title() + name.title()
        self.gumjs_field_prefix = "{0}_{1}".format(flavor, name)
        self.gumjs_function_prefix = "gumjs_{0}_{1}".format(flavor, name)
        self.module_struct_name = to_camel_case("gum_{0}_code_{1}".format(namespace, name), start_high=True)
        self.register_type = "GumX86Reg" if arch == "x86" else arch + "_reg"

class Api(object):
    def __init__(self, static_methods, instance_methods):
        self.static_methods = static_methods
        self.instance_methods = instance_methods

        native_register_type = None
        for method in instance_methods:
            reg_types = [arg.type for arg in method.args if arg.type_converter == "register"]
            if len(reg_types) > 0:
                native_register_type = reg_types[0]
                break
        self.native_register_type = native_register_type

class Method(object):
    def __init__(self, name, return_type, args):
        is_put_array = name.startswith("put_") and name.endswith("_array")
        if is_put_array:
            name = name[:-6]
        is_put_call = is_put_array and name.startswith("put_call_")
        is_put_regs = is_put_array and "_regs" in name

        self.name = name
        self.name_js = to_camel_case(name, start_high=False)

        self.is_put_array = is_put_array
        if is_put_array:
            args.pop(len(args) - 2)

        self.is_put_call = is_put_call
        if is_put_call:
            self.needs_calling_convention_arg = args[0].type == "GumCallingConvention"
            if self.needs_calling_convention_arg:
                args.pop(0)
        else:
            self.needs_calling_convention_arg = False

        self.is_put_regs = is_put_regs

        self.return_type = return_type
        if return_type == "void" or (return_type == "gboolean" and name.startswith("put_")):
            self.return_type_ts = "void"
        elif return_type == "gboolean":
            self.return_type_ts = "boolean"
        elif return_type == "guint":
            self.return_type_ts = "number"
        elif return_type in ("gpointer", "GumAddress"):
            self.return_type_ts = "NativePointer"
        elif return_type == "cs_insn *":
            self.return_type_ts = "Instruction | null"
        else:
            raise ValueError("Unsupported return type: {0}".format(return_type))
        self.args = args

class MethodArgument(object):
    def __init__(self, type, name, arch):
        self.type = type

        name_raw = None
        converter = None

        if type in ("GumX86Reg", "arm_reg", "arm64_reg", "mips_reg"):
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = to_camel_case("x86_register" if type == "GumX86Reg" else type.replace("_reg", "_register"), start_high=True)
            converter = "register"
        elif type in ("arm_sysreg",):
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "ArmSystemRegister"
            converter = "system_register"
        elif type in ("gint", "gint8", "gint16", "gint32"):
            self.type_raw = "gint"
            self.type_format = "i"
            self.type_ts = "number"
        elif type in ("guint", "guint8", "guint16", "guint32"):
            self.type_raw = "guint"
            self.type_format = "u"
            self.type_ts = "number"
        elif type == "gint64":
            self.type_raw = type
            self.type_format = "q"
            self.type_ts = "number | Int64"
        elif type == "guint64":
            self.type_raw = type
            self.type_format = "Q"
            self.type_ts = "number | UInt64"
        elif type == "gssize":
            self.type_raw = type
            self.type_format = "z"
            self.type_ts = "number | Int64 | UInt64"
        elif type == "gsize":
            self.type_raw = type
            self.type_format = "Z"
            self.type_ts = "number | Int64 | UInt64"
        elif type in ("gpointer", "gconstpointer", "gconstpointer *"):
            self.type_raw = type
            self.type_format = "p"
            self.type_ts = "NativePointerValue"
        elif type == "GumAddress":
            self.type_raw = "gpointer"
            self.type_format = "p"
            self.type_ts = "NativePointerValue"
            converter = "address"
        elif type == "$label":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "string"
            converter = "label"
        elif type == "$array":
            self.type_raw = "GBytes *"
            self.type_format = "B~"
            self.type_ts = "ArrayBuffer | number[] | string"
            converter = "bytes"
        elif type == "x86_insn":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "X86InstructionId"
            converter = "instruction_id"
        elif type == "GumCallingConvention":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "CallingConvention"
            converter = "calling_convention"
        elif type in ("const GumArgument *", "const arm_reg *"):
            self.type_raw = "$array"
            self.type_format = "A"
            if type == "const GumArgument *":
                self.type_ts = arch.title() + "CallArgument[]"
            else:
                self.type_ts = "ArmRegister[]"
            name = "items"
            name_raw = "items_value"
        elif type == "GumBranchHint":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "X86BranchHint"
            converter = "branch_hint"
        elif type == "GumX86PtrTarget":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "X86PointerTarget"
            converter = "pointer_target"
        elif type in ("arm_cc", "arm64_cc"):
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "ArmConditionCode" if type == "arm_cc" else "Arm64ConditionCode"
            converter = "condition_code"
        elif type == "arm_shifter":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "ArmShifter"
            converter = "shifter"
        elif type == "GumArm64IndexMode":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "Arm64IndexMode"
            converter = "index_mode"
        elif type == "GumRelocationScenario":
            self.type_raw = "const gchar *"
            self.type_format = "s"
            self.type_ts = "RelocationScenario"
            converter = "relocator_scenario"
        else:
            raise ValueError("Unhandled type: {0}".format(type))

        self.type_converter = converter

        if name_raw is None:
            name_raw = name if converter is None else "raw_{0}".format(name)

        self.name = name
        self.name_js = to_camel_case(name, start_high=False)
        self.name_raw = name_raw

    def name_raw_for_cpp(self):
        if self.type == "$label":
            return "raw_{0}".format(self.name)
        return self.name_raw

    def type_raw_for_cpp(self):
        if self.type_format == "s":
            return "std::string"
        return self.type_raw

    def type_format_for_cpp(self):
        if self.type_format == "s":
            return "S"
        return self.type_format

    def type_converter_for_cpp(self):
        if self.type == "$label":
            return "label"
        return self.type_converter

def parse_api(name, arch, flavor, api_header, options):
    static_methods = []
    instance_methods = []

    self_type = "{0} * ".format(to_camel_case("gum_{0}_{1}".format(flavor, name), start_high=True))
    ignored_methods = set(options.get('ignore', []))

    put_methods = [(m.group(2), m.group(1), m.group(3)) for m in re.finditer(r"GUM_API ([\w *]+) gum_{0}_{1}_([\w]+) \(([^)]+)\);".format(flavor, name), api_header)]
    for method_name, return_type, raw_arglist in put_methods:
        if method_name in ignored_methods:
            continue

        raw_args = [raw_arg.strip() for raw_arg in raw_arglist.replace("\n", " ").split(", ")]
        if raw_args[-1] == "...":
            continue

        is_static = not raw_args[0].startswith(self_type)

        if not is_static:
            raw_args = raw_args[1:]

        if not is_static and method_name == "put_bytes":
            args = [MethodArgument("$array", "data", arch)]
        else:
            args = [parse_arg(raw_arg, arch) for raw_arg in raw_args]

        method = Method(method_name, return_type, args)
        if is_static:
            static_methods.append(method)
        else:
            instance_methods.append(method)

    return Api(static_methods, instance_methods)

def parse_arg(raw_arg, arch):
    tokens = raw_arg.split(" ")
    raw_type = " ".join(tokens[0:-1])
    name = tokens[-1]
    if raw_type == "gconstpointer":
        if name in ("id", "label_id"):
            return MethodArgument("$label", name, arch)
        return MethodArgument(raw_type, name, arch)
    return MethodArgument(raw_type, name, arch)

def to_camel_case(name, start_high):
    result = ""
    uppercase_next = start_high
    for c in name:
        if c == "_":
            uppercase_next = True
        elif uppercase_next:
            result += c.upper()
            uppercase_next = False
        else:
            result += c.lower()
    return result


if __name__ == '__main__':
    main(sys.argv)
