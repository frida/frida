from __future__ import annotations

import textwrap
from pathlib import Path
from typing import Dict, List, Optional

from .model import (ClassObjectType, CustomTypeKind, Enumeration,
                    InterfaceObjectType, Method, Model, ObjectType, Parameter,
                    Procedure, Property, Signal, Tuple, to_camel_case,
                    to_pascal_case)


def primary_constructor(otype: ObjectType) -> Optional[Procedure]:
    if not otype.constructors:
        return None
    return next((c for c in otype.constructors if c.name == "new"), otype.constructors[0])


def factory_constructors(otype: ObjectType) -> List[Procedure]:
    primary = primary_constructor(otype)
    return [
        ctor
        for ctor in otype.constructors
        if ctor is not primary and all(not p.type.nick.endswith("array") for p in ctor.input_parameters)
    ]

ASSETS_DIR = Path(__file__).resolve().parent / "assets"
CODEGEN_HELPERS_TS = (ASSETS_DIR / "codegen_helpers.ts").read_text(encoding="utf-8")
CODEGEN_TYPES_H = (ASSETS_DIR / "codegen_types.h").read_text(encoding="utf-8")
CODEGEN_PROTOTYPES_H = (ASSETS_DIR / "codegen_prototypes.h").read_text(encoding="utf-8")
CODEGEN_HELPERS_C = (ASSETS_DIR / "codegen_helpers.c").read_text(encoding="utf-8")


def generate_all(model: Model) -> Dict[str, str]:
    return {
        "ts": generate_ts(model),
        "dts": generate_napi_dts(model),
        "c": generate_napi_bindings(model),
    }


def generate_ts(model: Model) -> str:
    type_imports = []
    for t in model.public_types.values():
        type_imports.append(f"{t.js_name} as _{t.js_name}")
        if isinstance(t, ObjectType):
            for s in t.signals:
                type_imports.append(f"{s.handler_type_name} as _{s.handler_type_name}")
                prefixed_name = s.prefixed_handler_type_name
                if prefixed_name != s.handler_type_name:
                    type_imports.append(f"{prefixed_name} as _{prefixed_name}")
        if isinstance(t, InterfaceObjectType) and t.has_abstract_base:
            type_imports.append(f"Abstract{t.js_name} as _Abstract{t.js_name}")
    for name in model.customizations.custom_types.keys():
        type_imports.append(f"{name} as _{name}")

    lines = [
        'import bindings from "bindings";',
        "import type {",
        "    FridaBinding,",
        *[f"    {i}," for i in type_imports],
        "    Signal,",
        "    SignalHandler,",
        '} from "./frida_binding.d.ts";',
        'import util from "util";',
        *model.customizations.helper_imports,
        "",
        "const { inspect } = util;",
    ]

    lines.append(
        """
const binding: FridaBinding = bindings({
    bindings: "frida_binding",
    try: [
        ["module_root", "build", "bindings"],
        [process.cwd(), "bindings"],
    ]
});"""
    )

    for name, cust in model.customizations.custom_types.items():
        if cust.kind == CustomTypeKind.ENUM:
            lines += [
                "",
                f"enum {name}Impl {{",
                indent_ts_code(cust.typing.strip(), 1),
                "}",
                f"(binding as any).{name} = {name}Impl;",
                f"export const {name} = binding.{name};",
            ]

    lines += [
        "",
        "{",
        indent_ts_code(model.customizations.helper_code.rstrip(), 1),
        indent_ts_code(CODEGEN_HELPERS_TS.rstrip(), 1),
    ]

    ol = []
    for otype in model.object_types.values():
        if not otype.is_frida_options and not otype.is_frida_list:
            prop_names = ", ".join([f'"{prop.js_name}"' for prop in otype.properties])
            ol += [
                "",
                f"(binding as any).{otype.prefixed_js_name}.prototype[inspect.custom] = function (depth: number, options: util.InspectOptionsStylized): string {{",
                f'    return inspectWrapper(this, "{otype.js_name}", [{prop_names}], depth, options);',
                "};",
            ]

        if not otype.needs_wrapper:
            continue

        ol += [
            "",
            f"class {otype.js_name} extends binding._{otype.js_name} {{",
        ]

        num_members = 0

        custom_code = otype.customizations.custom_code
        if custom_code is not None:
            for declaration in custom_code.declarations:
                if num_members != 0:
                    ol.append("")
                ol.append(indent_ts_code(declaration.code.strip(), 1))
                num_members += 1

            for method in custom_code.methods:
                if num_members != 0:
                    ol.append("")
                ol.append(indent_ts_code(method.code.strip(), 1))
                num_members += 1

        ctor = primary_constructor(otype)
        if ctor is not None and ctor.needs_wrapper:
            ol.append(f"    constructor({', '.join(ctor.param_typings)}) {{")

            custom_logic = ctor.customizations.custom_logic
            if custom_logic is not None:
                ol += [
                    indent_ts_code(custom_logic.strip(), 2),
                    "",
                ]

            ol += [
                f"        super({', '.join(param.js_name for param in ctor.parameters)});",
                "    }",
            ]

        for method in otype.wrapped_methods:
            custom = method.customizations

            maybe_async = "async " if method.is_async else ""
            maybe_await = "await " if method.is_async else ""

            if num_members != 0:
                ol.append("")
            ol.append(
                f"    {maybe_async}{method.js_name}({', '.join(method.param_typings)}): {method.return_typing} {{"
            )

            custom_logic = custom.custom_logic
            if custom_logic is not None:
                ol += [
                    indent_ts_code(custom_logic.strip(), 2),
                    "",
                ]

            return_capture = "const result = " if method.return_typing != "void" else ""

            ol.append(
                f"        {return_capture}{maybe_await}this._{method.js_name}({', '.join(param.js_name for param in method.input_parameters)});"
            )

            if return_capture:
                ol.append("")
                return_wrapper = custom.return_wrapper
                if return_wrapper is not None:
                    if return_wrapper.startswith("as "):
                        ol.append(f"        return result {return_wrapper};")
                    else:
                        ol.append(f"        return {return_wrapper}(result);")
                else:
                    ol.append("        return result;")

            ol.append("    }")

            num_members += 1

        for signal in otype.wrapped_signals:
            custom = signal.customizations

            option_lines = []

            transform = custom.transform
            if transform is not None:
                param_typings = []
                transformed_params = []
                for i, param in enumerate(signal.parameters):
                    if transform is not None and i in transform:
                        transformed_name_and_type, transform_function = transform[i]
                        param_typings.append(transformed_name_and_type)
                        if transform_function is not None:
                            transformed_params.append(
                                f"{transform_function}({param.js_name})"
                            )
                        else:
                            transformed_params.append(param.js_name)
                    else:
                        param_typings.append(param.typing)
                        transformed_params.append(param.js_name)

                transformed_params_str = ", ".join(transformed_params)

                option_lines += [
                    f"transform({', '.join(p.js_name for p in signal.parameters)}) {{",
                    f"    return [{transformed_params_str}];",
                    "},",
                ]

            intercept = custom.intercept
            if intercept is not None:
                option_lines.append(f"intercept: {intercept},")

            if num_members != 0:
                ol.append("")
            option_indent = 8 * " "
            ol += [
                f"    {signal.js_name}: Signal<_{signal.handler_type_name}> = new SignalWrapper<__{signal.handler_type_name}, _{signal.handler_type_name}>(this._{signal.js_name}, {{",
                *[option_indent + line for line in option_lines],
                "    });",
            ]

            num_members += 1

        ol += [
            "}",
            "",
            f"binding.{otype.js_name} = {otype.js_name};",
        ]

    lines += [
        indent_ts_code("\n".join(ol), 1),
        "}",
        "",
        "binding.commitConstructors();",
        "",
        "export const {",
    ]
    for t in model.public_types.values():
        if isinstance(t, InterfaceObjectType):
            if t.has_abstract_base:
                lines.append(f"    Abstract{t.js_name},")
            continue
        lines.append(f"    {t.js_name},")
    lines += [
        "} = binding;",
        "",
        "const frida = {",
    ]
    for t in model.public_types.values():
        if isinstance(t, InterfaceObjectType):
            if t.has_abstract_base:
                lines.append(f"    Abstract{t.js_name},")
            continue
        lines.append(f"    {t.js_name},")
    for name, cust in model.customizations.custom_types.items():
        if cust.kind == CustomTypeKind.ENUM:
            lines.append(f"    {name},")
    lines += [
        *[f"    {e}," for e in model.customizations.facade_exports],
        "} as const;",
        "",
        "export default frida;",
    ]

    type_exports = []
    for t in model.public_types.values():
        type_exports.append(f"export type {t.js_name} = _{t.js_name};")
        if isinstance(t, ObjectType):
            if isinstance(t, InterfaceObjectType) and t.has_abstract_base:
                type_exports.append(
                    f"export type Abstract{t.js_name} = _Abstract{t.js_name};"
                )
            type_exports += [
                f"export type {s.handler_type_name} = _{s.handler_type_name};"
                for s in t.signals
            ]
    for name in model.customizations.custom_types.keys():
        type_exports.append(f"export type {name} = _{name};")

    lines += [
        "",
        "namespace frida {",
    ]
    for e in type_exports:
        lines.append(indent_ts_code(e, 1))
    lines += [
        "}",
        "",
    ]
    for e in type_exports:
        lines.append(e)

    lines.append("")
    lines.append(model.customizations.facade_code)

    return "\n".join(lines)


def generate_napi_dts(model: Model) -> str:
    lines = [
        "export interface FridaBinding {",
        "    commitConstructors(): void;",
    ]
    for t in model.public_types.values():
        if t.is_frida_options:
            lines.append(f"    {t.prefixed_js_name}: {t.prefixed_js_name};")
        else:
            if isinstance(t, InterfaceObjectType):
                if t.has_abstract_base:
                    lines.append(
                        f"    Abstract{t.js_name}: typeof Abstract{t.js_name};"
                    )
                continue
            if isinstance(t, ClassObjectType):
                if t.needs_wrapper:
                    lines.append(f"    {t.js_name}: typeof {t.js_name};")
            lines.append(f"    {t.prefixed_js_name}: typeof {t.prefixed_js_name};")
    for name, cust in model.customizations.custom_types.items():
        if cust.kind == CustomTypeKind.ENUM:
            lines.append(f"    {name}: typeof {name};")
    lines.append("}")

    for otype in model.object_types.values():
        if not otype.is_public:
            continue

        if otype.needs_wrapper:
            lines += [
                "",
                f"export class {otype.js_name} extends {otype.prefixed_js_name} {{",
            ]

            custom_code = otype.customizations.custom_code
            if custom_code is not None:
                for declaration in custom_code.declarations:
                    typing = declaration.typing
                    if typing is not None:
                        lines.append(f"    {typing};")

                for method in custom_code.methods:
                    typing = method.typing
                    if typing is not None:
                        lines.append(f"    {typing};")

            ctor = primary_constructor(otype)
            if ctor is not None and ctor.needs_wrapper:
                lines.append(f"    constructor({', '.join(ctor.param_typings)});")

            for method in otype.wrapped_methods:
                params = ", ".join(method.param_typings)
                lines.append(f"    {method.js_name}({params}): {method.return_typing};")

            for signal in otype.wrapped_signals:
                lines.append(
                    f"    readonly {signal.js_name}: Signal<{signal.handler_type_name}>;"
                )

            lines.append("}")

            if otype.wrapped_signals:
                lines.append("")
                for signal in otype.wrapped_signals:
                    params = ", ".join(
                        signal.customizations.transform.get(i, (param.typing, ""))[0]
                        for i, param in enumerate(signal.parameters)
                    )
                    lines.append(
                        f"export type {signal.handler_type_name} = ({params}) => void;"
                    )

        lines.append("")

        parent = otype.parent
        parent_name = parent.js_name if parent is not None else None
        extends = (
            ""
            if (parent_name is None or otype.is_frida_options)
            else f" extends {parent_name}"
        )
        if isinstance(otype, InterfaceObjectType) or otype.is_frida_options:
            lines.append(f"export interface {otype.prefixed_js_name}{extends} {{")
        else:
            implements = (
                f" implements {', '.join([t.js_name for t in otype.implements])}"
                if otype.implements
                else ""
            )
            lines.append(
                f"export class {otype.prefixed_js_name}{extends}{implements} {{"
            )

            constructor = primary_constructor(otype)
            if constructor is not None:
                params = ", ".join(param.typing for param in constructor.parameters)
                if not constructor.parameters and otype.supports_settings:
                    params = f"settings?: {{ {' '.join(settings_typings(otype, model))} }}"
                lines.append(f"    constructor({params});")

            for ctor in factory_constructors(otype):
                params = ", ".join(param.typing for param in ctor.parameters)
                lines.append(f"    static {to_camel_case(ctor.name)}({params}): {otype.prefixed_js_name};")

        if otype.is_frida_options:
            for method in otype.methods:
                if method.is_select_method:
                    lines.append(
                        f"    {method.select_plural_noun}?: {model.resolve_js_type(method.select_element_type)}[];"
                    )
        else:
            for method in otype.methods:
                if method.is_property_accessor:
                    continue
                visibility = (
                    "protected " if method.prefixed_js_name != method.js_name else ""
                )
                lines.append(
                    f"    {visibility}{method.prefixed_js_name}({', '.join(method.prefixed_param_typings)}): {method.prefixed_return_typing};"
                )

        for prop in otype.properties:
            lines.append(f"    {prop.typing};")

        for signal in otype.signals:
            visibility = (
                "protected " if signal.prefixed_js_name != signal.js_name else ""
            )
            lines.append(
                f"    {visibility}readonly {signal.prefixed_js_name}: Signal<{signal.prefixed_handler_type_name}>;"
            )

        if isinstance(otype, ClassObjectType):
            for itype in otype.implements:
                for method in itype.methods:
                    lines.append(
                        f"    {method.js_name}({', '.join(method.param_typings)}): {method.return_typing};"
                    )

                for prop in itype.properties:
                    lines.append(f"    {prop.typing};")

                for signal in itype.signals:
                    lines.append(
                        f"    readonly {signal.js_name}: Signal<{signal.handler_type_name}>;"
                    )

        lines.append("}")

        if otype.signals:
            lines.append("")
            for signal in otype.signals:
                lines.append(
                    f"export type {signal.prefixed_handler_type_name} = {signal.typing};"
                )

        if isinstance(otype, InterfaceObjectType) and otype.has_abstract_base:
            lines.append("")

            object_js_name = model.resolve_object_type("Object").js_name
            lines.append(
                f"export abstract class Abstract{otype.js_name} extends {object_js_name} implements {otype.js_name} {{"
            )

            for method in otype.methods:
                params = ", ".join([t.replace("?:", ":") for t in method.param_typings])
                lines.append(f"    {method.js_name}({params}): {method.return_typing};")

            for prop in otype.properties:
                lines.append(f"    {prop.typing};")

            for signal in otype.signals:
                lines.append(
                    f"    readonly {signal.js_name}: Signal<{signal.handler_type_name}>;"
                )

            lines.append("}")

    for enum in model.enumerations.values():
        members = ",\n    ".join(
            f'{member.js_name} = "{member.nick}"' for member in enum.members
        )
        lines += [
            "",
            f"export enum {enum.js_name} {{",
            f"    {members}",
            "}",
        ]

    for name, cust in model.customizations.custom_types.items():
        code = f"\nexport {cust.kind.value} {name}"
        if cust.kind == CustomTypeKind.TYPE:
            code += " = "
            code += cust.typing.strip()
            code += ";"
        else:
            code += " {\n"
            code += indent_ts_code(cust.typing.strip(), 1)
            code += "\n}"
        lines.append(code)

    lines.append(
        """
export class Signal<H extends SignalHandler> {
    connect(handler: H): void;
    disconnect(handler: H): void;
}

export type SignalHandler = (...args: any[]) => void;
"""
    )

    return "\n".join(lines)


def generate_napi_bindings(model: Model) -> str:
    object_types = model.object_types.values()
    enumerations = model.enumerations.values()

    code = generate_includes()
    code += generate_abstract_base_type_declarations(model)
    code += generate_operation_structs(object_types)
    code += CODEGEN_TYPES_H
    code += generate_prototypes(object_types, enumerations)
    code += generate_abstract_base_define_type_invocations(model)
    code += generate_shared_globals()
    code += generate_type_tags(object_types)
    code += generate_constructor_declarations(object_types)
    code += generate_tsfn_declarations(object_types)
    code += generate_init_function(object_types, enumerations)
    code += generate_commit_constructors_function(object_types)

    for otype in object_types:
        if otype.is_frida_options:
            code += generate_options_conversion_functions(otype)
            continue
        if otype.is_frida_list:
            code += generate_list_conversion_functions(otype)
            continue

        code += generate_object_type_registration_code(otype, model)
        code += generate_object_type_conversion_functions(otype)
        if otype.supports_settings:
            code += generate_settings_conversion_functions(otype)
        code += generate_object_type_constructor(otype)
        for ctor in factory_constructors(otype):
            code += generate_object_type_factory(otype, ctor)
        code += generate_object_type_finalizer(otype)
        code += generate_object_type_cleanup_code(otype)

        for method in otype.methods:
            code += generate_method_code(method)

        for signal in otype.signals:
            code += generate_signal_getter_code(otype, signal)

        if isinstance(otype, InterfaceObjectType) and otype.has_abstract_base:
            code += generate_abstract_base_registration_code(otype)
            code += generate_abstract_base_constructor(otype)
            code += generate_abstract_base_gobject_glue(otype)
            for method in otype.methods:
                code += generate_abstract_base_method_code(method)

    for enum in enumerations:
        code += generate_enum_registration_code(enum)
        code += generate_enum_conversion_functions(enum)

    code += CODEGEN_HELPERS_C

    return code


def generate_includes() -> str:
    return """\
#include <frida-core.h>
#include <node_api.h>
#include <string.h>

"""


def generate_operation_structs(object_types: List[ObjectType]) -> str:
    structs = []

    for otype in object_types:
        is_iface_with_abstract_base = (
            isinstance(otype, InterfaceObjectType) and otype.has_abstract_base
        )

        for method in otype.methods:
            if method.is_async:
                param_declarations = generate_parameter_variable_declarations(method)
                return_declaration = generate_return_variable_declaration(method)
                decls = "".join(
                    [
                        indent_c_code(param_declarations, 1, prologue="\n"),
                        indent_c_code(return_declaration, 1, prologue="\n"),
                    ]
                )
                structs.append(
                    f"""\
typedef struct {{
  napi_deferred deferred;
  napi_threadsafe_function tsfn;
  {otype.c_type} * handle;{decls}
}} {method.operation_type_name};
"""
                )

            if is_iface_with_abstract_base:
                param_declarations = generate_parameter_variable_declarations(method)
                return_declaration = generate_return_variable_declaration(method)
                decls = "".join(
                    [
                        indent_c_code(param_declarations, 1, prologue="\n"),
                        indent_c_code(return_declaration, 1, prologue="\n"),
                    ]
                )
                structs.append(
                    f"""\
typedef struct {{
  {otype.abstract_base_c_type} * self;{decls}
}} {method.abstract_base_operation_type_name};
"""
                )

    return "\n".join(structs)


def generate_prototypes(
    object_types: List[ObjectType], enumerations: List[Enumeration]
) -> str:
    prototypes = [
        "static void fdn_deinit (void * data);",
        "",
        "static napi_value fdn_commit_constructors (napi_env env, napi_callback_info info);",
    ]

    for otype in object_types:
        otype_cprefix = otype.c_symbol_prefix

        prototypes.append("")

        if not otype.is_frida_options and not otype.is_frida_list:
            prototypes.append(
                f"static void {otype_cprefix}_register (napi_env env, napi_value exports);"
            )

        if not otype.is_frida_list:
            prototypes.append(
                f"G_GNUC_UNUSED static gboolean {otype_cprefix}_from_value (napi_env env, napi_value value, {otype.c_type} ** handle);"
            )

        if not otype.is_frida_options:
            prototypes += [
                f"G_GNUC_UNUSED static napi_value {otype_cprefix}_to_value (napi_env env, {otype.c_type} * handle);",
            ]

        if not otype.is_frida_options and not otype.is_frida_list:
            prototypes.append(
                f"static napi_value {otype_cprefix}_construct (napi_env env, napi_callback_info info);"
            )
            if otype.supports_settings:
                prototypes.append(
                    f"static gboolean {otype_cprefix}_apply_settings (napi_env env, {otype.c_type} * self, napi_value value);"
                )

            for ctor in factory_constructors(otype):
                prototypes.append(
                    f"static napi_value {otype_cprefix}_{ctor.name} (napi_env env, napi_callback_info info);"
                )

            custom = otype.customizations
            if custom is not None and custom.cleanup is not None:
                prototypes += [
                    f"static void {otype_cprefix}_finalize (napi_env env, void * finalize_data, void * finalize_hint);",
                    "",
                    f"static void {otype.c_symbol_prefix}_handle_cleanup (void * data);",
                ]

            for method in otype.methods:
                method_cprefix = f"{otype_cprefix}_{method.name}"
                prototypes += [
                    "",
                    f"static napi_value {method_cprefix} (napi_env env, napi_callback_info info);",
                ]
                if method.is_async:
                    prototypes += [
                        f"static gboolean {method_cprefix}_begin (gpointer user_data);",
                        f"static void {method_cprefix}_end (GObject * source_object, GAsyncResult * res, gpointer user_data);",
                        f"static void {method_cprefix}_deliver (napi_env env, napi_value js_cb, void * context, void * data);",
                        f"static void {method_cprefix}_operation_free ({method.operation_type_name} * operation);",
                    ]

            for i, signal in enumerate(otype.signals):
                if i == 0:
                    prototypes.append("")
                prototypes.append(
                    f"static napi_value {otype_cprefix}_get_{signal.c_name}_signal (napi_env env, napi_callback_info info);"
                )

        if isinstance(otype, InterfaceObjectType) and otype.has_abstract_base:
            cprefix = otype.abstract_base_c_symbol_prefix

            prototypes += [
                "",
                f"static void {cprefix}_register (napi_env env, napi_value exports);",
                f"static napi_value {cprefix}_construct (napi_env env, napi_callback_info info);",
                f"static void {cprefix}_iface_init (gpointer g_iface, gpointer iface_data);",
                f"static void {cprefix}_dispose (GObject * object);",
                f"static void {cprefix}_release_js_resources (napi_env env, napi_value js_cb, void * context, void * data);",
            ]

            for m in otype.methods:
                method_cprefix = f"{cprefix}_{m.name}"
                prototypes += [
                    f"static void {method_cprefix} ({', '.join(m.param_ctypings)});",
                    f"static void {method_cprefix}_operation_free ({m.abstract_base_operation_type_name} * operation);",
                    f"static void {method_cprefix}_begin (napi_env env, napi_value js_cb, void * context, void * data);",
                    f"static napi_value {method_cprefix}_on_success (napi_env env, napi_callback_info info);",
                    f"static napi_value {method_cprefix}_on_failure (napi_env env, napi_callback_info info);",
                    f"static {m.return_ctyping} {method_cprefix}_finish ({', '.join(m.finish_param_ctypings)});",
                ]

    for enum in enumerations:
        enum_cprefix = enum.c_symbol_prefix
        prototypes += [
            "",
            f"static void {enum_cprefix}_register (napi_env env, napi_value exports);",
            f"G_GNUC_UNUSED static gboolean {enum_cprefix}_from_value (napi_env env, napi_value value, {enum.c_type} * e);",
            f"G_GNUC_UNUSED static napi_value {enum_cprefix}_to_value (napi_env env, {enum.c_type} e);",
        ]

    prototypes.append(CODEGEN_PROTOTYPES_H.rstrip())

    return "\n".join(prototypes) + "\n\n"


def generate_shared_globals() -> str:
    return "\n".join(
        [
            "static napi_ref fdn_exports;",
            "static GHashTable * fdn_constructors;",
            "static gboolean fdn_in_cleanup = FALSE;",
            "",
            "",
        ]
    )


def generate_type_tags(object_types: List[ObjectType]) -> str:
    type_tags = [
        "static napi_type_tag fdn_handle_wrapper_type_tag = { 0xdd596d4f2dad45f9, 0x844585a48e8d05ba };",
        "static napi_type_tag fdn_object_type_tag = { 0x4eeacfcdc22c425a, 0x91346eafdc89fedc };",
    ]
    return "\n".join(type_tags) + "\n"


def generate_constructor_declarations(object_types: List[ObjectType]) -> str:
    declarations = []

    for otype in object_types:
        if otype.is_frida_options or otype.is_frida_list:
            continue
        declarations.append(f"static napi_ref {otype.c_symbol_prefix}_constructor;")

    declarations += [
        "",
        "static napi_ref fdn_signal_constructor;",
    ]

    return "\n" + "\n".join(declarations) + "\n"


def generate_tsfn_declarations(object_types: List[ObjectType]) -> str:
    declarations = []

    for otype in object_types:
        if isinstance(otype, InterfaceObjectType) and otype.has_abstract_base:
            declarations.append(
                f"static napi_threadsafe_function {otype.abstract_base_c_symbol_prefix}_release_js_resources_tsfn;"
            )
            for method in otype.methods:
                declarations.append(
                    f"static napi_threadsafe_function {otype.abstract_base_c_symbol_prefix}_{method.name}_tsfn;"
                )

    declarations += [
        "",
        "static napi_threadsafe_function fdn_keep_alive_tsfn;",
    ]

    return "\n".join(declarations) + "\n"


def generate_init_function(
    object_types: List[ObjectType], enumerations: List[Enumeration]
) -> str:
    object_type_prefixes = []
    for otype in object_types:
        if otype.is_frida_options or otype.is_frida_list:
            continue
        object_type_prefixes.append(otype.c_symbol_prefix)
        if isinstance(otype, InterfaceObjectType) and otype.has_abstract_base:
            object_type_prefixes.append(otype.abstract_base_c_symbol_prefix)
    object_type_registration_calls = "\n  ".join(
        [f"{prefix}_register (env, exports);" for prefix in object_type_prefixes]
    )

    enum_type_registration_calls = "\n  ".join(
        [f"{enum.c_symbol_prefix}_register (env, exports);" for enum in enumerations]
    )

    return f"""
static napi_value
fdn_init (napi_env env,
          napi_value exports)
{{
  napi_value commit_ctors;

  frida_init ();

  napi_create_reference (env, exports, 1, &fdn_exports);
  fdn_constructors = g_hash_table_new (NULL, NULL);

  napi_create_function (env, "commitConstructors", NAPI_AUTO_LENGTH, fdn_commit_constructors, NULL, &commit_ctors);
  napi_set_named_property (env, exports, "commitConstructors", commit_ctors);

  {object_type_registration_calls}

  {enum_type_registration_calls}

  fdn_signal_register (env, exports);

  napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, "FridaKeepAlive"), 0, 1, NULL, NULL, NULL, fdn_keep_alive_on_tsfn_invoke, &fdn_keep_alive_tsfn);
  napi_unref_threadsafe_function (env, fdn_keep_alive_tsfn);

  napi_add_env_cleanup_hook (env, fdn_deinit, NULL);

  return exports;
}}

static void
fdn_deinit (void * data)
{{
  fdn_in_cleanup = TRUE;
}}

NAPI_MODULE (NODE_GYP_MODULE_NAME, fdn_init)
"""


def generate_commit_constructors_function(object_types: List[ObjectType]) -> str:
    commits = ""
    for otype in object_types:
        if otype.is_frida_options or otype.is_frida_list:
            continue

        otype_cprefix = otype.c_symbol_prefix

        if otype.needs_wrapper:
            commits += f"""  if ({otype_cprefix}_constructor == NULL)
  {{
    napi_get_named_property (env, exports, "{otype.js_name}", &ctor);
    napi_create_reference (env, ctor, 1, &{otype_cprefix}_constructor);
  }}
"""

        commits += f"  g_hash_table_insert (fdn_constructors, GSIZE_TO_POINTER ({otype.get_type} ()), {otype_cprefix}_constructor);\n\n"

    inherits = ""
    for otype in object_types:
        if otype.is_frida_options or otype.is_frida_list:
            continue

        parent = otype.parent
        if parent is None:
            continue

        if otype.needs_wrapper:
            inherits += f"""
  napi_get_named_property (env, exports, "{otype.prefixed_js_name}", &ctor);
  fdn_inherit_ref_val (env, {otype.c_symbol_prefix}_constructor, ctor, object_ctor, set_proto);
"""
            if parent.name == "Object":
                inherits += "  fdn_inherit_val_val (env, ctor, fdn_object_ctor, object_ctor, set_proto);"
            else:
                inherits += "  fdn_inherit_val_ref (env, ctor, {parent.c_symbol_prefix}_constructor, object_ctor, set_proto);"
        else:
            if parent.name == "Object":
                inherits += f"""
  fdn_inherit_ref_val (env, {otype.c_symbol_prefix}_constructor, fdn_object_ctor, object_ctor, set_proto);
"""
            else:
                inherits += f"""
  fdn_inherit_ref_ref (env, {otype.c_symbol_prefix}_constructor, {parent.c_symbol_prefix}_constructor, object_ctor, set_proto);
"""

    return f"""
static napi_value
fdn_commit_constructors (napi_env env,
                         napi_callback_info info)
{{
  napi_value result, exports, ctor, global, object_ctor, set_proto, fdn_object_ctor;

  napi_get_reference_value (env, fdn_exports, &exports);
{commits}  napi_get_global (env, &global);
  napi_get_named_property (env, global, "Object", &object_ctor);
  napi_get_named_property (env, object_ctor, "setPrototypeOf", &set_proto);

  napi_get_reference_value (env, fdn_object_constructor, &fdn_object_ctor);
{inherits}
  napi_get_undefined (env, &result);
  return result;
}}
"""


def generate_object_type_registration_code(otype: ObjectType, model: Model) -> str:
    otype_cprefix = otype.c_symbol_prefix

    ctor_ref_creation = (
        ""
        if otype.needs_wrapper
        else f"\n  napi_create_reference (env, constructor, 1, &{otype_cprefix}_constructor);"
    )
    jsprop_registrations = []

    for ctor in factory_constructors(otype):
        jsprop_registrations.append(generate_factory_registration_entry(otype, ctor))

    for method in otype.methods:
        if method.is_property_accessor:
            continue
        jsprop_registrations.append(generate_method_registration_entry(method))

    for prop in otype.properties:
        jsprop_registrations.append(generate_property_registration_entry(prop))

    for signal in otype.signals:
        jsprop_registrations.append(generate_signal_registration_entry(signal))

    if isinstance(otype, ClassObjectType):
        for itype in otype.implements:
            for method in itype.methods:
                if method.is_property_accessor:
                    continue
                jsprop_registrations.append(generate_method_registration_entry(method))

            for prop in itype.properties:
                jsprop_registrations.append(generate_property_registration_entry(prop))

            for signal in itype.signals:
                jsprop_registrations.append(generate_signal_registration_entry(signal))

    if jsprop_registrations:
        jsprop_registrations_str = "\n    ".join(jsprop_registrations)
        properties_declaration = f"""  napi_property_descriptor properties[] =
  {{
    {jsprop_registrations_str}
  }};
"""
        properties_arguments = "G_N_ELEMENTS (properties), properties"
    else:
        properties_declaration = ""
        properties_arguments = "0, NULL"

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    two_newlines = "\n\n"

    return f"""
static void
{otype_cprefix}_register (napi_env env,
{calculate_indent("_register")}napi_value exports)
{{
{properties_declaration}  napi_value constructor;

  napi_define_class (env, "{otype.prefixed_js_name}", NAPI_AUTO_LENGTH, {otype_cprefix}_construct, NULL, {properties_arguments}, &constructor);{ctor_ref_creation}

  napi_set_named_property (env, exports, "{otype.prefixed_js_name}", constructor);
}}
"""


def generate_method_registration_entry(method: Method) -> str:
    return f'{{ "{method.prefixed_js_name}", NULL, {method.object_type.c_symbol_prefix}_{method.name}, NULL, NULL, NULL, napi_default, NULL }},'


def generate_factory_registration_entry(otype: ObjectType, ctor: Procedure) -> str:
    return f'{{ "{to_camel_case(ctor.name)}", NULL, {otype.c_symbol_prefix}_{ctor.name}, NULL, NULL, NULL, napi_static, NULL }},'


def generate_property_registration_entry(prop: Property) -> str:
    otype_cprefix = prop.object_type.c_symbol_prefix

    setter_str = f"{otype_cprefix}_{prop.setter}" if prop.setter is not None else "NULL"

    attrs = ["enumerable", "configurable"]
    if prop.setter is not None:
        attrs.insert(0, "writable")
    attrs_str = " | ".join([f"napi_{attr}" for attr in attrs])

    return f'{{ "{prop.js_name}", NULL, NULL, {otype_cprefix}_{prop.getter}, {setter_str}, NULL, {attrs_str}, NULL }},'


def generate_signal_registration_entry(signal: Signal) -> str:
    return f'{{ "{signal.prefixed_js_name}", NULL, NULL, {signal.object_type.c_symbol_prefix}_get_{signal.c_name}_signal, NULL, NULL, napi_default, NULL }},'


def generate_object_type_conversion_functions(otype: ObjectType) -> str:
    otype_cprefix = otype.c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    from_value_function = f"""
static gboolean
{otype_cprefix}_from_value (napi_env env,
{calculate_indent("_from_value")}napi_value value,
{calculate_indent("_from_value")}{otype.c_type} ** handle)
{{
  return fdn_object_unwrap (env, value, {otype.get_type} (), (GObject **) handle);
}}
"""

    to_value_function = f"""
static napi_value
{otype_cprefix}_to_value (napi_env env,
{calculate_indent("_to_value")}{otype.c_type} * handle)
{{
  return fdn_object_new (env, G_OBJECT (handle), {otype_cprefix}_constructor);
}}
"""

    return from_value_function + to_value_function


def generate_object_type_constructor(otype: ObjectType) -> str:
    otype_cprefix = otype.c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    ctor = primary_constructor(otype)

    n_parameters = max(len(ctor.parameters) if ctor is not None else 0, 1)

    storage_prefix = ""
    invalid_arg_label = "propagate_error"
    error_check = ""
    construction_failed_logic = ""

    if ctor is not None:
        param_declarations = generate_parameter_variable_declarations(
            ctor, initialize=True
        )
        if ctor.parameters:
            param_conversions = generate_input_parameter_conversions_code(
                ctor, storage_prefix, invalid_arg_label
            )
        elif otype.supports_settings:
            param_conversions = """if (argc > 1)
  goto invalid_handle;"""
        else:
            param_conversions = """if (argc != 0)
  goto invalid_handle;"""
        param_destructions = generate_parameter_destructions_code(ctor, storage_prefix)

        call_args = generate_call_arguments_code(ctor, storage_prefix)
        constructor_call = (
            f"handle = {otype.c_cast_macro} ({ctor.c_identifier} ({call_args}));"
        )
        if not ctor.parameters and otype.supports_settings:
            constructor_call += f"""

if (argc == 1 && !{otype.c_symbol_prefix}_apply_settings (env, handle, args[0]))
  goto propagate_error;"""
        unconstructable_logic = ""

        if ctor.throws:
            error_check = """if (error != NULL)
  goto construction_failed;"""
            construction_failed_logic = """construction_failed:
  {
    napi_throw (env, fdn_error_to_value (env, error));
    g_error_free (error);
    goto propagate_error;
  }
"""
    else:
        param_declarations = ""
        param_conversions = """if (argc == 0)
  goto unconstructable;

goto invalid_handle;"""
        param_destructions = ""
        constructor_call = ""
        unconstructable_logic = f"""unconstructable:
  {{
    napi_throw_error (env, NULL, "type {otype.js_name} cannot be constructed");
    return NULL;
  }}
"""

    if ctor is not None and ctor.parameters:
        invalid_handle_logic = ""
    else:
        invalid_handle_logic = f"""invalid_handle:
  {{
    napi_throw_type_error (env, NULL, "expected a {otype.js_name} handle");
    goto propagate_error;
  }}
"""

    custom = otype.customizations

    finalizer = "fdn_object_finalize"

    cleanup_code = ""
    if custom is not None and custom.cleanup is not None:
        cleanup_code = f"""

  napi_add_env_cleanup_hook (env, {otype_cprefix}_handle_cleanup, handle);
  g_object_set_data (G_OBJECT (handle), "fdn-cleanup-hook", {otype_cprefix}_handle_cleanup);"""
        finalizer = f"{otype_cprefix}_finalize"

    keep_alive_code = ""
    if custom is not None and custom.keep_alive is not None:
        keep_alive = custom.keep_alive
        method = next(
            (
                method
                for method in otype.methods
                if method.name == keep_alive.is_destroyed_function
            )
        )
        keep_alive_code = f"""

  fdn_keep_alive_until (env, jsthis, G_OBJECT (handle), (FdnIsDestroyedFunc) {method.c_identifier}, "{keep_alive.destroy_signal_name}");"""

    one_newline = "\n"
    two_newlines = "\n\n"

    return f"""
static napi_value
{otype_cprefix}_construct (napi_env env,
{calculate_indent("_construct")}napi_callback_info info)
{{
  napi_value result = NULL;
  size_t argc = {n_parameters};
  napi_value args[{n_parameters}], jsthis;
  bool is_instance;{indent_c_code(param_declarations, 1, prologue=one_newline)}
  {otype.c_type} * handle = NULL;

  if (napi_get_cb_info (env, info, &argc, args, &jsthis, NULL) != napi_ok)
    goto propagate_error;

  if (argc != 0 && napi_check_object_type_tag (env, args[0], &fdn_handle_wrapper_type_tag, &is_instance) == napi_ok && is_instance)
  {{
    if (napi_get_value_external (env, args[0], (void **) &handle) != napi_ok)
      goto propagate_error;

    g_object_ref (handle);
  }}
  else
  {{
{indent_c_code(param_conversions, 2)}{indent_c_code(constructor_call, 2, prologue=two_newlines)}{indent_c_code(error_check, 2, prologue=one_newline)}
  }}

  if (!fdn_object_wrap (env, jsthis, G_OBJECT (handle), {finalizer}))
    goto propagate_error;{cleanup_code}{keep_alive_code}

  result = jsthis;
  goto beach;

{unconstructable_logic}{invalid_handle_logic}{construction_failed_logic}propagate_error:
  {{
    g_clear_object (&handle);
    goto beach;
  }}
beach:
  {{{indent_c_code(param_destructions, 2, prologue=one_newline)}
    return result;
  }}
}}
"""


def generate_object_type_factory(otype: ObjectType, ctor: Procedure) -> str:
    otype_cprefix = otype.c_symbol_prefix
    fn_name = f"{otype_cprefix}_{ctor.name}"

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    n_parameters = max(len(ctor.parameters), 1)

    param_declarations = generate_parameter_variable_declarations(ctor, initialize=True)
    param_conversions = generate_input_parameter_conversions_code(ctor, "", "beach")
    param_destructions = generate_parameter_destructions_code(ctor, "")
    call_args = generate_call_arguments_code(ctor, "")

    error_check = ""
    construction_failed_logic = ""
    if ctor.throws:
        error_check = """if (error != NULL)
  goto construction_failed;"""
        construction_failed_logic = """construction_failed:
  {
    napi_throw (env, fdn_error_to_value (env, error));
    g_error_free (error);
    goto beach;
  }
"""

    one_newline = "\n"
    two_newlines = "\n\n"

    return f"""
static napi_value
{fn_name} (napi_env env,
{calculate_indent(f"_{ctor.name}")}napi_callback_info info)
{{
  napi_value result = NULL;
  size_t argc = {n_parameters};
  napi_value args[{n_parameters}];{indent_c_code(param_declarations, 1, prologue=one_newline)}
  {otype.c_type} * handle = NULL;

  if (napi_get_cb_info (env, info, &argc, args, NULL, NULL) != napi_ok)
    goto beach;

{indent_c_code(param_conversions, 1)}

  handle = {otype.c_cast_macro} ({ctor.c_identifier} ({call_args}));{indent_c_code(error_check, 1, prologue=one_newline)}

  result = fdn_object_new (env, G_OBJECT (handle), {otype_cprefix}_constructor);
  goto beach;

{construction_failed_logic}beach:
  {{
    g_clear_object (&handle);{indent_c_code(param_destructions, 2, prologue=one_newline)}
    return result;
  }}
}}
"""


def generate_object_type_finalizer(otype: ObjectType) -> str:
    custom = otype.customizations
    if custom is None or custom.cleanup is None:
        return ""

    otype_cprefix = otype.c_symbol_prefix

    indent = " " * (len(otype_cprefix) + len("_finalize") + 2)

    return f"""
static void
{otype_cprefix}_finalize (napi_env env,
{indent}void * finalize_data,
{indent}void * finalize_hint)
{{
  {otype.c_type} * self = finalize_data;

  if (g_object_steal_data (G_OBJECT (self), "fdn-cleanup-hook") != NULL)
    napi_remove_env_cleanup_hook (env, {otype_cprefix}_handle_cleanup, self);

  fdn_object_finalize (env, finalize_data, finalize_hint);
}}
"""


def generate_object_type_cleanup_code(otype: ObjectType) -> str:
    custom = otype.customizations
    if custom is None or custom.cleanup is None:
        return ""

    cleanup_method = next(
        (method for method in otype.methods if method.name == custom.cleanup)
    )

    return f"""
static void
{otype.c_symbol_prefix}_handle_cleanup (void * data)
{{
  {otype.c_type} * self = data;

  g_object_steal_data (G_OBJECT (self), "fdn-cleanup-hook");

  {cleanup_method.c_identifier}_sync (self, NULL, NULL);
}}
"""


def generate_method_code(method: Method) -> str:
    otype = method.object_type
    operation_type_name = method.operation_type_name
    otype_cprefix = otype.c_symbol_prefix

    storage_prefix = "operation->" if method.is_async else ""
    invalid_arg_label = "invalid_argument" if method.is_async else "beach"

    if method.input_parameters:
        args_declarations = f"""\
size_t argc = {len(method.input_parameters)};
napi_value args[{len(method.input_parameters)}];"""
        get_cb_info_argc_args = "&argc, args"
    else:
        args_declarations = ""
        get_cb_info_argc_args = "NULL, NULL"

    param_conversions = generate_input_parameter_conversions_code(
        method, storage_prefix, invalid_arg_label
    )
    param_destructions = generate_parameter_destructions_code(method, storage_prefix)

    return_assignment = generate_return_assignment_code(method, storage_prefix)
    return_conversion = generate_return_conversion_code(method, storage_prefix)
    return_destruction = generate_return_destruction_code(method, storage_prefix)

    keep_alive_code = ""
    custom = method.customizations
    if custom is not None:
        if custom.ref_keep_alive:
            keep_alive_code = (
                "\n\nnapi_ref_threadsafe_function (env, fdn_keep_alive_tsfn);"
            )
        elif custom.unref_keep_alive:
            keep_alive_code = (
                "\n\nnapi_unref_threadsafe_function (env, fdn_keep_alive_tsfn);"
            )

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + 1 + len(method.name) + len(suffix) + 2)

    one_newline = "\n"
    two_newlines = "\n\n"

    if method.is_async:
        operation_free_function = f"""\
static void
{otype_cprefix}_{method.name}_operation_free ({operation_type_name} * operation)
{{{indent_c_code(param_destructions, 1, prologue=one_newline)}{indent_c_code(return_destruction, 1, prologue=one_newline)}
  g_slice_free ({operation_type_name}, operation);
}}"""

        code = f"""
static napi_value
{otype_cprefix}_{method.name} (napi_env env,
{calculate_indent('')}napi_callback_info info)
{{{indent_c_code(args_declarations, 1, prologue=one_newline)}
  napi_value jsthis;
  {otype.c_type} * handle;
  napi_deferred deferred;
  napi_value promise;
  {operation_type_name} * operation;
  GSource * source;

  if (napi_get_cb_info (env, info, {get_cb_info_argc_args}, &jsthis, NULL) != napi_ok)
    return NULL;

  if (napi_unwrap (env, jsthis, (void **) &handle) != napi_ok)
    return NULL;

  napi_create_promise (env, &deferred, &promise);

  operation = g_slice_new0 ({operation_type_name});
  operation->deferred = deferred;
  napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, "{method.prefixed_js_name}"), 0, 1, NULL, NULL, NULL,
      {otype_cprefix}_{method.name}_deliver, &operation->tsfn);
  operation->handle = handle;
  operation->error = NULL;{indent_c_code(param_conversions, 1, prologue=two_newlines)}

  source = g_idle_source_new ();
  g_source_set_callback (source, {otype_cprefix}_{method.name}_begin,
      operation, NULL);
  g_source_attach (source, frida_get_main_context ());
  g_source_unref (source);

  return promise;

invalid_argument:
  {{
    napi_reject_deferred (env, deferred, NULL);
    {otype_cprefix}_{method.name}_operation_free (operation);
    return NULL;
  }}
}}

static gboolean
{otype_cprefix}_{method.name}_begin (gpointer user_data)
{{
  {operation_type_name} * operation = user_data;

  {method.c_identifier} (operation->handle,
      {", ".join([f"operation->{param.name}" for param in method.parameters])},
      {otype_cprefix}_{method.name}_end, operation);

  return G_SOURCE_REMOVE;
}}

static void
{otype_cprefix}_{method.name}_end (GObject * source_object,
{calculate_indent("_end")}GAsyncResult * res,
{calculate_indent("_end")}gpointer user_data)
{{
  {operation_type_name} * operation = user_data;

  {return_assignment}{method.finish_c_identifier} (operation->handle, res, &operation->error);

  napi_call_threadsafe_function (operation->tsfn, operation, napi_tsfn_blocking);
}}

static void
{otype_cprefix}_{method.name}_deliver (napi_env env,
{calculate_indent("_deliver")}napi_value js_cb,
{calculate_indent("_deliver")}void * context,
{calculate_indent("_deliver")}void * data)
{{
  {operation_type_name} * operation = data;

  if (operation->error != NULL)
  {{
    napi_value error_obj = fdn_error_to_value (env, operation->error);
    napi_reject_deferred (env, operation->deferred, error_obj);
    g_error_free (operation->error);
  }}
  else
  {{
    napi_value js_retval;
{indent_c_code(return_conversion, 2)}
    napi_resolve_deferred (env, operation->deferred, js_retval);{indent_c_code(keep_alive_code, 2)}
  }}

  napi_release_threadsafe_function (operation->tsfn, napi_tsfn_release);

  {otype_cprefix}_{method.name}_operation_free (operation);
}}

{operation_free_function}
"""
    else:
        param_declarations = generate_parameter_variable_declarations(
            method, initialize=True
        )
        return_declaration = generate_return_variable_declaration(method)
        call_args = generate_call_arguments_code(
            method, storage_prefix, instance_arg="handle"
        )

        if method.throws:
            error_check = """if (error != NULL)
  goto call_failed;"""
            call_failed_logic = """call_failed:
  {
    napi_throw (env, fdn_error_to_value (env, error));
    g_error_free (error);
    goto beach;
  }
"""
        else:
            error_check = ""
            call_failed_logic = ""

        post_call_logic = "".join(
            [
                indent_c_code(error_check, 1, prologue=one_newline),
                indent_c_code(keep_alive_code, 1, prologue=two_newlines),
                indent_c_code(return_conversion, 1, prologue=two_newlines),
                indent_c_code(return_destruction, 1, prologue=one_newline),
            ]
        )

        code = f"""
static napi_value
{otype_cprefix}_{method.name} (napi_env env,
{calculate_indent('')}napi_callback_info info)
{{
  napi_value js_retval = NULL;{indent_c_code(args_declarations, 1, prologue=one_newline)}
  napi_value jsthis;
  {otype.c_type} * handle;{indent_c_code(param_declarations, 1, prologue=one_newline)}{indent_c_code(return_declaration, 1, prologue=one_newline)}

  if (napi_get_cb_info (env, info, {get_cb_info_argc_args}, &jsthis, NULL) != napi_ok)
    goto beach;

  if (napi_unwrap (env, jsthis, (void **) &handle) != napi_ok)
    goto beach;{indent_c_code(param_conversions, 1, prologue=two_newlines)}

  {return_assignment}{method.c_identifier} ({call_args});{post_call_logic}
  goto beach;

{call_failed_logic}beach:
  {{{indent_c_code(param_destructions, 2, prologue=one_newline)}
    return js_retval;
  }}
}}
"""
    return code


def generate_parameter_variable_declarations(
    proc: Procedure, initialize: bool = False
) -> str:
    decls = []

    for param in proc.parameters:
        line = f"{param.type.c.replace('const ', '')} {param.name}"
        if initialize:
            default_val = param.type.default_value
            if default_val is not None:
                line += f" = {default_val}"
        line += ";"
        decls.append(line)

    if proc.throws:
        line = "GError * error"
        if initialize:
            line += " = NULL"
        line += ";"
        decls.append(line)

    return "\n".join(decls)


def generate_input_parameter_conversions_code(
    proc: Procedure, storage_prefix: str, invalid_arg_label: str
) -> str:
    conversions = [
        generate_parameter_conversion_code(param, i, storage_prefix, invalid_arg_label)
        for i, param in enumerate(proc.input_parameters)
    ]
    return "\n\n".join(conversions)


def generate_parameter_destructions_code(proc: Procedure, storage_prefix: str) -> str:
    destructions = [
        generate_parameter_destruction_code(param, storage_prefix)
        for param in proc.parameters
    ]
    return "\n".join([d for d in destructions if d is not None])


def generate_parameter_conversion_code(
    param: Parameter, index: int, storage_prefix: str, invalid_arg_label: str
) -> str:
    code = f"""\
if (argc > {index} && !fdn_is_undefined_or_null (env, args[{index}]))
{{
  if (!fdn_{param.type.nick}_from_value (env, args[{index}], &{storage_prefix}{param.name}))
    goto {invalid_arg_label};
}}
else
{{
"""

    if param.nullable:
        code += f"  {storage_prefix}{param.name} = NULL;"
    else:
        code += f"""  napi_throw_type_error (env, NULL, "missing argument: {param.js_name}");
  goto {invalid_arg_label};"""

    code += "\n}"

    return code


def generate_parameter_destruction_code(
    param: Parameter, storage_prefix: str
) -> Optional[str]:
    func = param.destroy_func
    if func is None:
        return None
    return generate_destruction_code(f"{storage_prefix}{param.name}", func)


def generate_call_arguments_code(
    proc: Procedure, storage_prefix: str, instance_arg: Optional[str] = None
) -> str:
    names = []
    if instance_arg is not None:
        names.append(instance_arg)
    names += [f"{storage_prefix}{param.name}" for param in proc.parameters]
    if proc.throws:
        names.append(f"&{storage_prefix}error")
    return ", ".join(names)


def generate_return_variable_declaration(method: Method) -> str:
    return (
        f"{method.return_value.type.c} retval;"
        if method.return_value is not None
        else ""
    )


def generate_return_assignment_code(method: Method, storage_prefix: str) -> str:
    return f"{storage_prefix}retval = " if method.return_value is not None else ""


def generate_return_conversion_code(method: Method, storage_prefix: str) -> str:
    if method.return_value is not None:
        custom = method.customizations
        if custom is not None and custom.return_cconversion is not None:
            code = f"js_retval = {custom.return_cconversion};"
        else:
            code = f"js_retval = fdn_{method.return_value.type.nick}_to_value (env, {storage_prefix}retval);"
        if method.return_value.nullable:
            code = f"if ({storage_prefix}retval != NULL)\n  {code}\nelse\n  napi_get_null (env, &js_retval);"
    else:
        code = "napi_get_undefined (env, &js_retval);"
    return code


def generate_return_destruction_code(method: Method, storage_prefix: str) -> str:
    retval = method.return_value
    if retval is None:
        return ""
    func = retval.destroy_func
    if func is None:
        return ""
    return generate_destruction_code(f"{storage_prefix}retval", func)


def generate_destruction_code(variable: str, destroy_func: str):
    if destroy_func == "g_free":
        return f"g_free ({variable});"
    return f"g_clear_pointer (&{variable}, {destroy_func});"


def generate_signal_getter_code(otype: ObjectType, signal: Signal) -> str:
    cprefix = otype.c_symbol_prefix

    custom = signal.customizations
    behavior = custom.behavior if custom is not None else "FDN_SIGNAL_ALLOW_EXIT"

    indent = " " * (len(cprefix) + 5 + len(signal.c_name) + 9)

    return f"""
static napi_value
{cprefix}_get_{signal.c_name}_signal (napi_env env,
{indent}napi_callback_info info)
{{
  return fdn_object_get_signal (env, info, "{signal.name}", "_{signal.prefixed_js_name}", {behavior});
}}
"""


def generate_abstract_base_type_declarations(model: Model) -> str:
    decls = []
    for itype in model.interface_types_with_abstract_base:
        ctype = itype.abstract_base_c_type
        cprefix = itype.abstract_base_c_symbol_prefix
        module_upper, obj_name_upper = cprefix.upper().split("_", maxsplit=1)
        decls.append(
            f"""G_DECLARE_FINAL_TYPE ({ctype}, {cprefix}, {module_upper}, {obj_name_upper}, GObject)

struct _{ctype}
{{
  GObject parent;

  napi_ref wrapper;
  gint disposed;
}};
"""
        )
    return "\n".join(decls) + "\n"


def generate_abstract_base_define_type_invocations(model: Model) -> str:
    invocations = []
    for itype in model.interface_types_with_abstract_base:
        cprefix = itype.abstract_base_c_symbol_prefix
        invocations.append(
            f"""G_DEFINE_TYPE_EXTENDED ({itype.abstract_base_c_type},
                        {cprefix},
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE ({itype.get_type} (),
                            {cprefix}_iface_init))"""
        )
    return "\n\n".join(invocations) + "\n\n"


def generate_abstract_base_registration_code(otype: ObjectType) -> str:
    otype_cprefix = otype.abstract_base_c_symbol_prefix

    tsfn_initializations = [
        f"""\
napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, "cleanup"), 0, 1, NULL, NULL, NULL, {otype_cprefix}_release_js_resources, &{otype_cprefix}_release_js_resources_tsfn);
napi_unref_threadsafe_function (env, {otype_cprefix}_release_js_resources_tsfn);"""
    ]

    for method in otype.methods:
        if method.is_property_accessor:
            continue
        tsfn_initializations.append(
            f"""\
napi_create_threadsafe_function (env, NULL, NULL, fdn_utf8_to_value (env, "{method.prefixed_js_name}"), 0, 1, NULL, NULL, NULL, {otype_cprefix}_{method.name}_begin, &{otype_cprefix}_{method.name}_tsfn);
napi_unref_threadsafe_function (env, {otype_cprefix}_{method.name}_tsfn);"""
        )

    tsfn_initializations_code = "\n\n".join(tsfn_initializations)

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    two_newlines = "\n\n"

    return f"""
static void
{otype_cprefix}_register (napi_env env,
{calculate_indent("_register")}napi_value exports)
{{
  napi_value constructor;
  napi_define_class (env, "Abstract{otype.js_name}", NAPI_AUTO_LENGTH, {otype_cprefix}_construct, NULL, 0, NULL, &constructor);

  napi_set_named_property (env, exports, "Abstract{otype.js_name}", constructor);{indent_c_code(tsfn_initializations_code, 1, prologue=two_newlines)}
}}
"""


def generate_abstract_base_constructor(otype: ObjectType) -> str:
    otype_cprefix = otype.abstract_base_c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    return f"""
static napi_value
{otype_cprefix}_construct (napi_env env,
{calculate_indent("_construct")}napi_callback_info info)
{{
  napi_value jsthis;
  {otype.abstract_base_c_type} * handle = NULL;

  if (napi_get_cb_info (env, info, NULL, NULL, &jsthis, NULL) != napi_ok)
    goto propagate_error;

  handle = g_object_new ({otype_cprefix}_get_type (), NULL);

  if (!fdn_object_wrap (env, jsthis, G_OBJECT (handle), fdn_object_finalize))
    goto propagate_error;

  napi_create_reference (env, jsthis, 1, &handle->wrapper);

  return jsthis;

propagate_error:
  {{
    g_clear_object (&handle);
    return NULL;
  }}
}}
"""


def generate_abstract_base_gobject_glue(otype: ObjectType) -> str:
    otype_cprefix = otype.abstract_base_c_symbol_prefix
    ctype = otype.abstract_base_c_type

    vmethod_lines = []
    for m in otype.methods:
        vmethod_lines += [
            f"iface->{m.name} = {otype_cprefix}_{m.name};",
            f"iface->{m.name}_finish = {otype_cprefix}_{m.name}_finish;",
        ]
    vmethod_assignments = indent_c_code("\n".join(vmethod_lines), 1)

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(suffix) + 2)

    return f"""
static void
{otype_cprefix}_class_init ({ctype}Class * klass)
{{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = {otype_cprefix}_dispose;
}}

static void
{otype_cprefix}_iface_init (gpointer g_iface,
{calculate_indent("_iface_init")}gpointer iface_data)
{{
  {otype.type_struct} * iface = g_iface;

{vmethod_assignments}
}}

static void
{otype_cprefix}_init ({ctype} * self)
{{
  self->disposed = FALSE;
}}

static void
{otype_cprefix}_dispose (GObject * object)
{{
  {otype.abstract_base_c_type} * self = {otype.abstract_base_c_cast_macro} (object);

  if (g_atomic_int_compare_and_exchange (&self->disposed, FALSE, TRUE) && !fdn_in_cleanup)
  {{
    napi_call_threadsafe_function ({otype_cprefix}_release_js_resources_tsfn, g_object_ref (self), napi_tsfn_blocking);
  }}

  G_OBJECT_CLASS ({otype_cprefix}_parent_class)->dispose (object);
}}

static void
{otype_cprefix}_release_js_resources (napi_env env,
{calculate_indent("_release_js_resources")}napi_value js_cb,
{calculate_indent("_release_js_resources")}void * context,
{calculate_indent("_release_js_resources")}void * data)
{{
  {otype.abstract_base_c_type} * self = data;

  napi_delete_reference (env, self->wrapper);
  self->wrapper = NULL;

  g_object_unref (self);
}}
"""


def generate_abstract_base_method_code(method: Method) -> str:
    otype = method.object_type
    operation_type_name = method.abstract_base_operation_type_name
    otype_cprefix = otype.abstract_base_c_symbol_prefix

    method_name_pascal = to_pascal_case(method.name)
    method_cprefix = f"{otype_cprefix}_{method.name}"

    def calculate_indent(suffix: str) -> str:
        return " " * (len(otype_cprefix) + len(method.name) + len(suffix) + 3)

    params = f",\n{calculate_indent('')}".join(method.param_ctypings)
    finish_params = f",\n{calculate_indent('_finish')}".join(
        method.finish_param_ctypings
    )

    storage_prefix = "operation->"

    param_assignments = generate_abstract_base_input_parameter_assignment_code(
        method, storage_prefix
    )
    param_conversions = generate_abstract_base_input_parameter_conversions_code(
        method, storage_prefix
    )
    param_destructions = generate_parameter_destructions_code(method, storage_prefix)
    cancellable_name = next(
        (p.name for p in method.parameters if p.type.name == "Gio.Cancellable"), "NULL"
    )

    result_declaration = generate_return_variable_declaration(method)
    result_conversion, result_destroy = generate_abstract_base_return_conversion_code(
        method, "propagate_error"
    )

    finish_error = "error" if method.throws else "NULL"
    finish_statement = f"g_task_propagate_pointer (G_TASK (result), {finish_error})"
    retval = method.return_value
    if retval is not None:
        from_pointer_func = retval.type.from_pointer_func
        if from_pointer_func is not None:
            finish_statement = f"{from_pointer_func} ({finish_statement})"
        finish_statement = f"return {finish_statement}"
    finish_code = f"{finish_statement};"

    one_newline = "\n"
    two_newlines = "\n\n"

    operation_free_function = f"""\
static void
{method_cprefix}_operation_free ({operation_type_name} * operation)
{{{indent_c_code(param_destructions, 1, prologue=one_newline)}
  g_slice_free ({operation_type_name}, operation);
}}"""

    return f"""
static void
{method_cprefix} ({params})
{{
  {otype.abstract_base_c_type} * self;
  {operation_type_name} * operation;
  GTask * task;

  self = {otype.abstract_base_c_cast_macro} ({method.cself_name});

  operation = g_slice_new0 ({operation_type_name});
  operation->self = self;{indent_c_code(param_assignments, 1, prologue=one_newline)}

  task = g_task_new (self, {cancellable_name}, callback, user_data);
  g_task_set_task_data (task, operation, (GDestroyNotify) {otype_cprefix}_{method.name}_operation_free);

  napi_call_threadsafe_function ({otype_cprefix}_{method.name}_tsfn, task, napi_tsfn_blocking);
}}

{operation_free_function}

static void
{method_cprefix}_begin (napi_env env,
{calculate_indent("_begin")}napi_value js_cb,
{calculate_indent("_begin")}void * context,
{calculate_indent("_begin")}void * data)
{{
  GTask * task = data;
  {operation_type_name} * operation;
  {otype.abstract_base_c_type} * self;
  napi_value wrapper, method, args[{len(method.input_parameters)}], js_retval, then, then_args[2], then_retval;

  operation = g_task_get_task_data (task);
  self = operation->self;

  if (napi_get_reference_value (env, self->wrapper, &wrapper) != napi_ok)
    goto propagate_error;

  if (napi_get_named_property (env, wrapper, "{method.prefixed_js_name}", &method) != napi_ok)
    goto propagate_error;{indent_c_code(param_conversions, 1, prologue=two_newlines)}

  if (napi_call_function (env, wrapper, method, G_N_ELEMENTS (args), args, &js_retval) != napi_ok)
    goto propagate_error;

  if (napi_get_named_property (env, js_retval, "then", &then) != napi_ok)
    goto propagate_error;

  napi_create_function (env, "on{method_name_pascal}Success", NAPI_AUTO_LENGTH, {otype_cprefix}_{method.name}_on_success, task, &then_args[0]);
  napi_create_function (env, "on{method_name_pascal}Failure", NAPI_AUTO_LENGTH, {otype_cprefix}_{method.name}_on_failure, task, &then_args[1]);

  if (napi_call_function (env, js_retval, then, G_N_ELEMENTS (then_args), then_args, &then_retval) != napi_ok)
    goto propagate_error;

  return;

propagate_error:
  {{
    napi_value js_error;
    GError * error;

    napi_get_and_clear_last_exception (env, &js_error);
    fdn_error_from_value (env, js_error, &error);

    g_task_return_error (task, error);
    g_object_unref (task);

    return;
  }}
}}

static napi_value
{method_cprefix}_on_success (napi_env env,
{calculate_indent("_on_success")}napi_callback_info info)
{{
  size_t argc = 1;
  napi_value js_retval;
  GTask * task;{indent_c_code(result_declaration, 1, prologue=one_newline)}
  gpointer raw_result;

  if (napi_get_cb_info (env, info, &argc, &js_retval, NULL, (void **) &task) != napi_ok)
    goto propagate_error;
  if (argc != 1)
    goto internal_error;

{indent_c_code(result_conversion, 1)}

  g_task_return_pointer (task, raw_result, {result_destroy});
  goto beach;

propagate_error:
  {{
    napi_value js_error;
    GError * error;

    napi_get_and_clear_last_exception (env, &js_error);
    fdn_error_from_value (env, js_error, &error);

    g_task_return_error (task, error);

    goto beach;
  }}
internal_error:
  {{
    g_task_return_new_error (task, FRIDA_ERROR, FRIDA_ERROR_INVALID_OPERATION,
        "Internal error");

    goto beach;
  }}
beach:
  {{
    napi_value val;

    g_object_unref (task);

    napi_get_undefined (env, &val);
    return val;
  }}
}}

static napi_value
{method_cprefix}_on_failure (napi_env env,
{calculate_indent("_on_failure")}napi_callback_info info)
{{
  size_t argc = 1;
  napi_value js_error;
  GTask * task;
  GError * error;

  if (napi_get_cb_info (env, info, &argc, &js_error, NULL, (void **) &task) != napi_ok)
    goto propagate_error;
  if (argc != 1)
    goto internal_error;

  if (!fdn_error_from_value (env, js_error, &error))
    goto propagate_error;

  g_task_return_error (task, error);
  goto beach;

propagate_error:
  {{
    napi_value js_error;
    GError * error;

    napi_get_and_clear_last_exception (env, &js_error);
    fdn_error_from_value (env, js_error, &error);

    g_task_return_error (task, error);

    goto beach;
  }}
internal_error:
  {{
    g_task_return_new_error (task, FRIDA_ERROR, FRIDA_ERROR_INVALID_OPERATION,
        "Internal error");

    goto beach;
  }}
beach:
  {{
    napi_value val;

    g_object_unref (task);

    napi_get_undefined (env, &val);
    return val;
  }}
}}

static {method.return_ctyping}
{method_cprefix}_finish ({finish_params})
{{
  {finish_code}
}}
"""


def generate_abstract_base_input_parameter_assignment_code(
    proc: Procedure, storage_prefix: str
) -> str:
    assigments = [
        generate_abstract_base_parameter_assignment_code(param, i, storage_prefix)
        for i, param in enumerate(proc.input_parameters)
    ]
    return "\n".join(assigments)


def generate_abstract_base_input_parameter_conversions_code(
    proc: Procedure, storage_prefix: str
) -> str:
    conversions = [
        generate_abstract_base_parameter_conversion_code(param, i, storage_prefix)
        for i, param in enumerate(proc.input_parameters)
    ]
    return "\n\n".join(conversions)


def generate_abstract_base_parameter_conversion_code(
    param: Parameter, index: int, storage_prefix: str
) -> str:
    lval = f"{storage_prefix}{param.name}"

    code = f"args[{index}] = fdn_{param.type.nick}_to_value (env, {lval});"
    if param.nullable:
        code = f"if ({lval} != NULL)\n  {code}\nelse\n  napi_get_null (env, &args[{index}]);"

    return code


def generate_abstract_base_parameter_assignment_code(
    param: Parameter, index: int, storage_prefix: str
) -> str:
    lval = f"{storage_prefix}{param.name}"

    copy_func = param.copy_func
    if copy_func is not None:
        if param.nullable and copy_func not in {"g_strdup", "g_strdupv"}:
            return (
                f"{lval} = ({param.name} != NULL) ? {copy_func} ({param.name}) : NULL;"
            )
        return f"{lval} = {copy_func} ({param.name});"

    return f"{lval} = {param.name};"


def generate_abstract_base_return_conversion_code(
    method: Method, invalid_label: str
) -> Tuple[str, str]:
    retval = method.return_value
    if retval is not None:
        destroy_func = retval.destroy_func
        if destroy_func is None:
            destroy_func = "NULL"

        result_conversion = (
            f"{retval.type.to_pointer_func} (retval)"
            if retval.type.to_pointer_func is not None
            else "retval"
        )

        code = f"""\
if (!fdn_{retval.type.nick}_from_value (env, js_retval, &retval))
  goto {invalid_label};

raw_result = {result_conversion};"""

        if retval.nullable:
            code = f"if (!fdn_is_null (env, js_retval))\n{{\n{indent_c_code(code, 1)}\n}} else {{\n  raw_result = NULL;\n}}"

    else:
        code = "raw_result = NULL;"
        destroy_func = "NULL"

    return (code, destroy_func)


def generate_enum_registration_code(enum: Enumeration) -> str:
    cprefix = enum.c_symbol_prefix

    properties = []
    for member in enum.members:
        properties.append(
            f'{{ "{member.js_name}", NULL, NULL, NULL, NULL, fdn_utf8_to_value (env, "{member.nick}"), napi_enumerable, NULL }}'
        )

    properties_str = ",\n    ".join(properties)

    def calculate_indent(suffix: str) -> str:
        return " " * (len(cprefix) + len(suffix) + 2)

    return f"""
static void
{cprefix}_register (napi_env env,
{calculate_indent("_register")}napi_value exports)
{{
  napi_value enum_object;
  napi_property_descriptor properties[] = {{
    {properties_str}
  }};

  napi_create_object (env, &enum_object);
  napi_define_properties (env, enum_object, G_N_ELEMENTS (properties), properties);

  napi_set_named_property (env, exports, "{enum.js_name}", enum_object);
}}
"""


def generate_enum_conversion_functions(enum: Enumeration) -> str:
    cprefix = enum.c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(cprefix) + len(suffix) + 2)

    return f"""
static gboolean
{cprefix}_from_value (napi_env env,
{calculate_indent("_from_value")}napi_value value,
{calculate_indent("_from_value")}{enum.c_type} * e)
{{
  return fdn_enum_from_value (env, {enum.get_type} (), value, (gint *) e);
}}

static napi_value
{cprefix}_to_value (napi_env env,
{calculate_indent("_to_value")}{enum.c_type} e)
{{
  return fdn_enum_to_value (env, {enum.get_type} (), e);
}}
"""


def settings_typings(otype: ObjectType, model) -> List[str]:
    typings = []
    current = otype
    while current is not None:
        for method in current.methods:
            if not method.is_select_method:
                continue
            if method.is_mapping_select_method:
                value_type = model.resolve_js_type(method.parameters[1].type)
                typings.append(f"{method.select_plural_noun}?: {{ [name: string]: {value_type} }};")
            else:
                element_type = model.resolve_js_type(method.select_element_type)
                typings.append(f"{method.select_plural_noun}?: {element_type}[];")
        for prop in current.settable_properties:
            typings.append(f"{prop.js_name}?: {model.resolve_js_type(prop.type)};")
        current = current.parent
    return typings


def generate_options_conversion_functions(otype: ObjectType) -> str:
    cprefix = otype.c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(cprefix) + len(suffix) + 2)

    selection_code = generate_selection_code(otype, "opts")

    cleanup_code = ""
    if selection_code:
        cleanup_code += """

propagate_error:
  {
    g_object_unref (opts);
    return FALSE;
  }"""

    return f"""
static gboolean
{cprefix}_from_value (napi_env env,
{calculate_indent("_from_value")}napi_value value,
{calculate_indent("_from_value")}{otype.c_type} ** options)
{{
  {otype.c_type} * opts;

  if (!fdn_options_from_value (env, {otype.get_type} (), value, (gpointer *) &opts))
    return FALSE;{selection_code}

  *options = opts;
  return TRUE;{cleanup_code}
}}
"""


def generate_settings_conversion_functions(otype: ObjectType) -> str:
    cprefix = otype.c_symbol_prefix

    def calculate_indent(suffix: str) -> str:
        return " " * (len(cprefix) + len(suffix) + 2)

    selection_code = generate_selection_code(otype, "self")

    cleanup_code = ""
    if selection_code:
        cleanup_code += """

propagate_error:
  {
    return FALSE;
  }"""

    selector_keys = selection_keys(otype)
    if selector_keys:
        keys_literal = "".join(f'"{key}", ' for key in selector_keys)
        skipped_keys_declaration = f"  static const gchar * const skipped_keys[] = {{ {keys_literal}NULL }};\n\n"
        skipped_keys_argument = "skipped_keys"
    else:
        skipped_keys_declaration = ""
        skipped_keys_argument = "NULL"

    return f"""
static gboolean
{cprefix}_apply_settings (napi_env env,
{calculate_indent("_apply_settings")}{otype.c_type} * self,
{calculate_indent("_apply_settings")}napi_value value)
{{
{skipped_keys_declaration}  if (!fdn_object_apply_properties (env, G_OBJECT (self), value, {skipped_keys_argument}))
    return FALSE;{selection_code}

  return TRUE;{cleanup_code}
}}
"""


def selection_keys(otype: ObjectType) -> List[str]:
    keys = []
    current = otype
    while current is not None:
        keys += [m.select_plural_noun for m in current.methods if m.is_select_method]
        current = current.parent
    return keys


def generate_selection_code(otype: ObjectType, target: str) -> str:
    code = ""
    current_otype = otype
    while current_otype is not None:
        target_expr = target if current_otype is otype else f"{current_otype.c_cast_macro} ({target})"

        for method in current_otype.methods:
            if not method.is_select_method:
                continue

            plural_noun = method.select_plural_noun

            if method.is_mapping_select_method:
                code += generate_mapping_selection_code(method, plural_noun, target_expr)
                continue

            param_type = method.select_element_type
            param_from_value = f"fdn_{param_type.nick}_from_value"

            element_destroy_code = ""
            destroy_func = param_type.destroy_func
            if destroy_func is not None:
                element_destroy_code = f"\n\n        {destroy_func} (element);"

            code += f"""

  {{
    napi_value js_{plural_noun};
    napi_valuetype value_type;

    if (napi_get_named_property (env, value, "{plural_noun}", &js_{plural_noun}) != napi_ok)
      goto propagate_error;

    if (napi_typeof (env, js_{plural_noun}, &value_type) != napi_ok)
      goto propagate_error;

    if (value_type != napi_undefined)
    {{
      uint32_t length, i;

      if (napi_get_array_length (env, js_{plural_noun}, &length) != napi_ok)
        goto propagate_error;

      for (i = 0; i != length; i++)
      {{
        napi_value js_element;
        {param_type.c.replace('const ', '')} element;

        if (napi_get_element (env, js_{plural_noun}, i, &js_element) != napi_ok)
          goto propagate_error;

        if (!{param_from_value} (env, js_element, &element))
          goto propagate_error;

        {method.c_identifier} ({target_expr}, element);{element_destroy_code}
      }}
    }}
  }}"""

        current_otype = current_otype.parent

    return code


def generate_mapping_selection_code(method, plural_noun: str, target_expr: str) -> str:
    key_type, value_type = [param.type for param in method.parameters]
    value_from_value = f"fdn_{value_type.nick}_from_value"

    element_destroy_code = ""
    destroy_func = value_type.destroy_func
    if destroy_func is not None:
        element_destroy_code = f"\n\n        {destroy_func} (element);"

    return f"""

  {{
    napi_value js_{plural_noun};
    napi_valuetype value_type;

    if (napi_get_named_property (env, value, "{plural_noun}", &js_{plural_noun}) != napi_ok)
      goto propagate_error;

    if (napi_typeof (env, js_{plural_noun}, &value_type) != napi_ok)
      goto propagate_error;

    if (value_type != napi_undefined)
    {{
      napi_value js_keys;
      uint32_t length, i;

      if (napi_get_property_names (env, js_{plural_noun}, &js_keys) != napi_ok)
        goto propagate_error;

      if (napi_get_array_length (env, js_keys, &length) != napi_ok)
        goto propagate_error;

      for (i = 0; i != length; i++)
      {{
        napi_value js_key, js_element;
        gchar * key;
        {value_type.c.replace('const ', '')} element;

        if (napi_get_element (env, js_keys, i, &js_key) != napi_ok)
          goto propagate_error;

        if (!fdn_utf8_from_value (env, js_key, &key))
          goto propagate_error;

        if (napi_get_property (env, js_{plural_noun}, js_key, &js_element) != napi_ok)
        {{
          g_free (key);
          goto propagate_error;
        }}

        if (!{value_from_value} (env, js_element, &element))
        {{
          g_free (key);
          goto propagate_error;
        }}

        {method.c_identifier} ({target_expr}, key, element);{element_destroy_code}

        g_free (key);
      }}
    }}
  }}"""


def generate_list_conversion_functions(otype: ObjectType) -> str:
    cprefix = otype.c_symbol_prefix

    size_method = next((method for method in otype.methods if method.name == "size"))
    get_method = next((method for method in otype.methods if method.name == "get"))

    element_type = get_method.return_value.type

    def calculate_indent(suffix: str) -> str:
        return " " * (len(cprefix) + len(suffix) + 2)

    return f"""
static napi_value
{cprefix}_to_value (napi_env env,
{calculate_indent("_to_value")}{otype.c_type} * list)
{{
  napi_value result;
  gint size, i;

  size = {size_method.c_identifier} (list);
  napi_create_array_with_length (env, size, &result);

  for (i = 0; i != size; i++)
  {{
    {element_type.c} handle = {get_method.c_identifier} (list, i);
    napi_set_element (env, result, i, fdn_{element_type.nick}_to_value (env, handle));
    g_object_unref (handle);
  }}

  return result;
}}
"""


def indent_ts_code(code: str, level: int, prologue: str = "") -> str:
    prefix = (level * 4) * " "
    return indent_code(code, prefix, prologue)


def indent_c_code(code: str, level: int, prologue: str = "") -> str:
    prefix = (level * 2) * " "
    return indent_code(code, prefix, prologue)


def indent_code(code: str, prefix: str, prologue: str = "") -> str:
    if not code:
        return ""
    return prologue + textwrap.indent(code, prefix, lambda line: line.strip() != "")
