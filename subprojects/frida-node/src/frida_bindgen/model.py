from __future__ import annotations

from collections import OrderedDict, defaultdict
from dataclasses import dataclass, field
from enum import Enum
from functools import cached_property
from typing import List, Mapping, Optional, Tuple, Union

import frida_bindgen_core as core
from frida_bindgen_core import (Direction, Namespace, Procedure,
                                TransferOwnership, Type)
from frida_bindgen_core.model import NUMERIC_GIR_TYPES
from frida_bindgen_core.naming import (to_camel_case, to_macro_case,
                                       to_pascal_case, to_snake_case)


class Model(core.Model):
    @cached_property
    def public_types(self) -> OrderedDict[str, Union[ObjectType, Enumeration]]:
        return OrderedDict(
            [(k, v) for k, v in self.object_types.items() if v.is_public]
            + list(self.enumerations.items())
        )

    @cached_property
    def interface_types_with_abstract_base(self) -> List[InterfaceObjectType]:
        return [
            t
            for t in self.object_types.values()
            if isinstance(t, InterfaceObjectType) and t.has_abstract_base
        ]

    def resolve_js_type(self, t: Type) -> str:
        js = js_type_from_gir(t.name)
        otype = self.object_types.get(js)
        if otype is not None:
            return otype.js_name
        return js


class ObjectType(core.ObjectType):
    @cached_property
    def js_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.js_name is not None:
            return custom.js_name
        return self.name

    @cached_property
    def prefixed_js_name(self) -> str:
        return f"_{self.js_name}" if self.needs_wrapper else self.js_name

    @cached_property
    def abstract_base_c_type(self) -> str:
        return f"FdnAbstract{self.name}"

    @property
    def is_public(self) -> bool:
        return not self.is_frida_list

    @cached_property
    def needs_wrapper(self) -> bool:
        custom = self.customizations
        if custom is None:
            return False
        if custom.custom_code is not None:
            return True
        ctor = self.constructors[0] if self.constructors else None
        if ctor is not None and ctor.needs_wrapper:
            return True
        return self.wrapped_methods or self.wrapped_signals

    @cached_property
    def c_symbol_prefix(self) -> str:
        return f"fdn_{to_snake_case(self.name)}"

    @cached_property
    def abstract_base_c_symbol_prefix(self) -> str:
        return f"fdn_abstract_{to_snake_case(self.name)}"

    @cached_property
    def c_cast_macro(self) -> str:
        return to_macro_case(self.c_type)

    @cached_property
    def abstract_base_c_cast_macro(self) -> str:
        return to_macro_case(self.abstract_base_c_type)

    @cached_property
    def supports_settings(self) -> bool:
        if self.is_frida_options or self.is_frida_list:
            return False
        ctor = self.constructors[0] if self.constructors else None
        if ctor is None or ctor.parameters:
            return False
        return bool(self.settable_properties) or any(m.is_select_method for m in self.methods)

    @cached_property
    def settable_properties(self) -> List[Property]:
        return [p for p in self.properties if p.writable and not p.construct_only]

    @cached_property
    def wrapped_methods(self) -> List[Method]:
        return [m for m in self.methods if m.needs_wrapper]

    @cached_property
    def wrapped_signals(self) -> List[Signal]:
        return [s for s in self.signals if s.needs_wrapper]


@dataclass
class ClassObjectType(ObjectType):
    _implements: List[str] = field(default_factory=list)

    @cached_property
    def implements(self) -> List[InterfaceObjectType]:
        return [self.model.resolve_object_type(i) for i in self._implements]


class InterfaceObjectType(ObjectType):
    @cached_property
    def has_abstract_base(self) -> bool:
        custom = self.customizations
        if custom is None:
            return True
        return not custom.drop_abstract_base


class Constructor(core.Constructor):
    @cached_property
    def param_typings(self) -> List[str]:
        custom = self.customizations
        if custom is not None and custom.param_typings is not None:
            return custom.param_typings
        return [param.typing for param in self.parameters]

    @property
    def needs_wrapper(self) -> bool:
        custom = self.customizations
        if custom is None:
            return False
        return custom.custom_logic is not None

    @cached_property
    def customizations(self) -> Optional[ConstructorCustomizations]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.constructor


class Method(core.Method):
    @cached_property
    def js_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.js_name is not None:
            return custom.js_name
        return to_camel_case(self.name)

    @cached_property
    def prefixed_js_name(self) -> str:
        custom = self.customizations
        if self.needs_wrapper or (custom is not None and custom.hide):
            return f"_{self.js_name}"
        return self.js_name

    @cached_property
    def cself_name(self) -> str:
        return to_snake_case(self.object_type.name).split("_")[-1]

    @cached_property
    def param_ctypings(self) -> List[str]:
        result = [f"{self.object_type.c_type} * {self.cself_name}"]
        result += [param.ctyping for param in self.parameters]
        if self.is_async:
            result += ["GAsyncReadyCallback callback", "gpointer user_data"]
        return result

    @cached_property
    def finish_param_ctypings(self) -> List[str]:
        result = [
            f"{self.object_type.c_type} * {self.cself_name}",
            "GAsyncResult * result",
        ]
        if self.throws:
            result.append("GError ** error")
        return result

    @cached_property
    def param_typings(self) -> List[str]:
        custom = self.customizations
        if custom is not None and custom.param_typings is not None:
            return custom.param_typings
        return self.prefixed_param_typings

    @cached_property
    def prefixed_param_typings(self) -> List[str]:
        return [param.typing for param in self.input_parameters]

    @cached_property
    def return_ctyping(self) -> str:
        retval = self.return_value
        return retval.ctyping if retval is not None else "void"

    @cached_property
    def return_typing(self) -> str:
        custom = self.customizations
        if custom is not None and custom.return_typing is not None:
            return custom.return_typing
        return self.prefixed_return_typing

    @cached_property
    def prefixed_return_typing(self) -> str:
        retval = self.return_value
        typing = retval.typing if retval is not None else "void"
        return f"Promise<{typing}>" if self.is_async else typing

    @property
    def needs_wrapper(self) -> bool:
        custom = self.customizations
        if custom is None:
            return False
        return custom.custom_logic is not None or custom.return_wrapper is not None

    @cached_property
    def customizations(self) -> Optional[MethodCustomizations]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.methods.get(self.name)

    @cached_property
    def operation_type_name(self) -> str:
        return f"Fdn{self.object_type.name}{to_pascal_case(self.name)}Operation"

    @cached_property
    def abstract_base_operation_type_name(self) -> str:
        return f"FdnAbstract{self.object_type.name}{to_pascal_case(self.name)}Operation"

    @cached_property
    def is_select_method(self) -> bool:
        if not (self.name.startswith("select_") or self.name.startswith("add_")):
            return False
        n_params = len(self.parameters)
        if n_params == 1:
            return True
        return n_params == 2 and self.parameters[0].type.name == "utf8"

    @cached_property
    def is_mapping_select_method(self) -> bool:
        return self.is_select_method and len(self.parameters) == 2

    @cached_property
    def select_noun(self) -> str:
        assert (
            self.is_select_method
        ), "select_noun can only be called on selector methods"
        return self.name.split("_", maxsplit=1)[1]

    @cached_property
    def select_plural_noun(self) -> str:
        return to_camel_case(f"{self.select_noun}s")

    @cached_property
    def select_element_type(self) -> Type:
        assert (
            self.is_select_method
        ), "select_element_type can only be called on selector methods"
        return self.parameters[0].type


class Property(core.Property):
    @cached_property
    def js_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.js_name is not None:
            return custom.js_name
        return to_camel_case(self.c_name)

    @cached_property
    def typing(self) -> str:
        custom = self.customizations
        if custom is not None and custom.typing is not None:
            return custom.typing
        readonly = "readonly " if not self.writable else ""
        optional_str = "?" if self.object_type.is_frida_options else ""
        return f"{readonly}{self.js_name}{optional_str}: {self.object_type.model.resolve_js_type(self.type)}"

    @cached_property
    def customizations(self) -> Optional[PropertyCustomizations]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.properties.get(self.name)


class Signal(core.Signal):
    @cached_property
    def js_name(self) -> str:
        return to_camel_case(self.c_name)

    @cached_property
    def prefixed_js_name(self) -> str:
        return f"_{self.js_name}" if self.needs_wrapper else self.js_name

    @cached_property
    def handler_type_name(self) -> str:
        # XXX: Special-cases to avoid breaking API:
        class_name = self.object_type.name
        if class_name == "DeviceManager":
            prefix = "Device"
        elif class_name == "Device":
            prefix = "Device" if self.name == "lost" else ""
        elif class_name == "PortalService":
            prefix = "Portal"
        elif class_name == "Cancellable":
            prefix = ""
        else:
            prefix = class_name
        return f"{prefix}{to_pascal_case(self.c_name)}Handler"

    @cached_property
    def prefixed_handler_type_name(self) -> str:
        return (
            f"_{self.handler_type_name}"
            if self.needs_wrapper
            else self.handler_type_name
        )

    @cached_property
    def typing(self) -> str:
        params = ", ".join([p.typing for p in self.parameters])
        return f"({params}) => void"

    @property
    def needs_wrapper(self) -> bool:
        custom = self.customizations
        if custom is None:
            return False
        return custom.transform is not None or custom.intercept is not None

    @cached_property
    def customizations(self) -> Optional[SignalCustomizations]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.signals.get(self.name)


class Parameter(core.Parameter):
    @cached_property
    def js_name(self) -> str:
        return to_camel_case(self.name)

    @cached_property
    def ctyping(self) -> str:
        return f"{self.type.c} {self.name}"

    @cached_property
    def typing(self) -> str:
        optional_str = "?" if self.optional else ""
        t = f"{self.js_name}{optional_str}: {self.object_type.model.resolve_js_type(self.type)}"
        if self.nullable and not self.type.is_frida_options:
            t += " | null"
        return t

    @cached_property
    def copy_func(self) -> Optional[str]:
        return self.type.copy_func

    @cached_property
    def destroy_func(self) -> Optional[str]:
        return self.type.destroy_func


class ReturnValue(core.ReturnValue):
    @cached_property
    def ctyping(self) -> str:
        return self.type.c

    @cached_property
    def typing(self) -> str:
        t = self.object_type.model.resolve_js_type(self.type)
        if self.nullable:
            t += " | null"
        return t

    @cached_property
    def destroy_func(self) -> Optional[str]:
        if self.transfer_ownership == TransferOwnership.none:
            return None
        return self.type.destroy_func


class Enumeration(core.Enumeration):
    @property
    def js_name(self) -> str:
        return self.name

    @property
    def prefixed_js_name(self) -> str:
        return self.name

    @property
    def is_frida_options(self) -> bool:
        return False

    @cached_property
    def c_symbol_prefix(self) -> str:
        return f"fdn_{to_snake_case(self.name)}"


class EnumerationMember(core.EnumerationMember):
    @cached_property
    def js_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.js_name is not None:
            return custom.js_name
        return to_pascal_case(self.name)

    @cached_property
    def nick(self) -> str:
        return self.name.replace("_", "-")

    @cached_property
    def customizations(self) -> Optional[EnumerationMemberCustomizations]:
        custom = self.enumeration.customizations
        if custom is None:
            return None
        return custom.members.get(self.name)


@dataclass
class Customizations:
    custom_types: Mapping[str, CustomType] = field(default_factory=OrderedDict)
    type_customizations: Mapping[str, TypeCustomizations] = field(
        default_factory=OrderedDict
    )
    facade_exports: List[str] = field(default_factory=list)
    facade_code: str = ""
    helper_imports: List[str] = field(default_factory=list)
    helper_code: str = ""


@dataclass
class CustomType:
    kind: CustomTypeKind
    typing: str


class CustomTypeKind(Enum):
    TYPE = "type"
    INTERFACE = "interface"
    ENUM = "enum"


@dataclass
class TypeCustomizations:
    pass


@dataclass
class ObjectTypeCustomizations(TypeCustomizations):
    js_name: Optional[str] = None
    drop: bool = False
    drop_abstract_base: bool = False
    constructor: Optional[ConstructorCustomizations] = None
    methods: Mapping[str, MethodCustomizations] = field(
        default_factory=lambda: defaultdict(dict)
    )
    properties: Mapping[str, PropertyCustomizations] = field(
        default_factory=lambda: defaultdict(dict)
    )
    signals: Mapping[str, SignalCustomizations] = field(
        default_factory=lambda: defaultdict(dict)
    )
    custom_code: Optional[CustomCode] = None
    cleanup: Optional[str] = None
    keep_alive: Optional[KeepAliveCustomization] = None


@dataclass
class KeepAliveCustomization:
    is_destroyed_function: str
    destroy_signal_name: str


@dataclass
class ConstructorCustomizations:
    drop: bool = False
    param_typings: Optional[List[str]] = None
    custom_logic: Optional[str] = None


@dataclass
class MethodCustomizations:
    js_name: Optional[str] = None
    drop: bool = False
    hide: bool = False
    param_typings: Optional[List[str]] = None
    return_typing: Optional[str] = None
    custom_logic: Optional[str] = None
    return_wrapper: Optional[str] = None
    return_cconversion: Optional[str] = None
    ref_keep_alive: bool = False
    unref_keep_alive: bool = False


@dataclass
class PropertyCustomizations:
    js_name: Optional[str] = None
    drop: bool = False
    typing: Optional[str] = None


@dataclass
class SignalCustomizations:
    drop: bool = False
    behavior: str = "FDN_SIGNAL_ALLOW_EXIT"
    transform: Optional[Mapping[int, Tuple[str, Optional[str]]]] = None
    intercept: Optional[str] = None


@dataclass
class CustomCode:
    declarations: List[CustomDeclaration] = field(default_factory=list)
    methods: List[CustomMethod] = field(default_factory=list)


@dataclass
class CustomDeclaration:
    typing: Optional[str]
    code: str


@dataclass
class CustomMethod:
    typing: Optional[str]
    code: str


@dataclass
class EnumerationCustomizations(TypeCustomizations):
    members: Mapping[str, EnumerationMemberCustomizations] = field(
        default_factory=lambda: defaultdict(dict)
    )


@dataclass
class EnumerationMemberCustomizations:
    js_name: Optional[str] = None


def _make_class(
    *,
    name,
    c_type,
    get_type,
    type_struct,
    parent,
    constructors,
    methods,
    properties,
    signals,
    implements,
    resolve_type,
    model,
):
    return ClassObjectType(
        name,
        c_type,
        get_type,
        type_struct,
        parent,
        constructors,
        methods,
        properties,
        signals,
        resolve_type,
        model,
        implements,
    )


def _make_interface(
    *,
    name,
    c_type,
    get_type,
    type_struct,
    parent,
    constructors,
    methods,
    properties,
    signals,
    resolve_type,
    model,
):
    return InterfaceObjectType(
        name,
        c_type,
        get_type,
        type_struct,
        parent,
        constructors,
        methods,
        properties,
        signals,
        resolve_type,
        model,
    )


FACTORY = core.Factory(
    class_object_type=_make_class,
    interface_object_type=_make_interface,
    constructor=Constructor,
    method=Method,
    parameter=Parameter,
    return_value=ReturnValue,
    signal=Signal,
    property_=Property,
    enumeration=Enumeration,
    enumeration_member=EnumerationMember,
    model=Model,
)


def js_type_from_gir(name: str) -> str:
    if name == "gboolean":
        return "boolean"
    if name in {"gint64", "guint64"}:
        return "bigint"
    if name in NUMERIC_GIR_TYPES:
        return "number"
    if name == "utf8":
        return "string"
    if name == "utf8[]":
        return "string[]"
    if name == "GLib.Bytes":
        return "Buffer"
    if name == "GLib.HashTable":
        return "VariantDict"
    if name == "GLib.Variant":
        return "any"
    if name in {"Gio.File", "Gio.TlsCertificate"}:
        return "string"
    if name.startswith("Frida.") and name.endswith("List"):
        return name[6:-4] + "[]"
    return name.split(".")[-1]
