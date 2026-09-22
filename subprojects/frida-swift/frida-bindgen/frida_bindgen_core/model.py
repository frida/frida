from __future__ import annotations

import xml.etree.ElementTree as ET
from collections import OrderedDict
from dataclasses import dataclass
from enum import Enum
from functools import cached_property
from typing import (Callable, Iterator, List, Optional, Sequence, Tuple)

from .naming import to_snake_case

CORE_NAMESPACE = "http://www.gtk.org/introspection/core/1.0"
C_NAMESPACE = "http://www.gtk.org/introspection/c/1.0"
GLIB_NAMESPACE = "http://www.gtk.org/introspection/glib/1.0"
GIR_NAMESPACES = {"": CORE_NAMESPACE, "glib": GLIB_NAMESPACE}

CORE_TAG_PREFIX = f"{{{CORE_NAMESPACE}}}"

NUMERIC_GIR_TYPES = {
    "gsize",
    "gssize",
    "gint",
    "guint",
    "glong",
    "gulong",
    "gint8",
    "gint16",
    "gint32",
    "gint64",
    "guint8",
    "guint16",
    "guint32",
    "guint64",
    "gfloat",
    "gdouble",
    "GType",
    "GQuark",
}

PRIMITIVE_GIR_TYPES = NUMERIC_GIR_TYPES | {
    "gpointer",
    "gboolean",
    "gchar",
    "utf8",
    "utf8[]",
}

ResolveTypeCallback = Callable[[str], Tuple[str, ET.Element]]


@dataclass
class Factory:
    class_object_type: Callable
    interface_object_type: Callable
    constructor: Callable
    method: Callable
    parameter: Callable
    return_value: Callable
    signal: Callable
    property_: Callable
    enumeration: Callable
    enumeration_member: Callable
    model: Callable


@dataclass
class Model:
    namespace: Namespace
    _object_types: OrderedDict
    enumerations: OrderedDict
    customizations: Optional[object] = None
    error_domain: Optional[object] = None
    factory: Optional[Factory] = None

    @cached_property
    def object_types(self) -> OrderedDict:
        result = OrderedDict()
        if self.customizations is not None:
            type_customizations = self.customizations.type_customizations
        else:
            type_customizations = {}
        for k, v in self._object_types.items():
            custom = type_customizations.get(k)
            if custom is None or not custom.drop:
                result[k] = v
        return result

    def resolve_object_type(self, name: str) -> ObjectType:
        bare_name = name.split(".", maxsplit=1)[-1]
        return self.object_types[bare_name]


@dataclass
class Namespace:
    name: str
    identifier_prefixes: str
    element: ET.Element

    @cached_property
    def type_elements(self):
        result = {}
        for toplevel in self.element.findall("./*[@name]", GIR_NAMESPACES):
            name = toplevel.get("name")
            result[name] = toplevel
            for callback in toplevel.findall("./callback", GIR_NAMESPACES):
                result[name + callback.get("name")] = callback
        return result


@dataclass
class ObjectType:
    name: str
    c_type: str
    get_type: str
    type_struct: str
    _parent: Optional[str]
    _constructors: List[ET.Element]
    _methods: List[ET.Element]
    _properties: List[ET.Element]
    _signals: List[ET.Element]
    resolve_type: ResolveTypeCallback

    model: Optional[Model]

    @cached_property
    def parent(self) -> ObjectType:
        if self._parent is None:
            return None
        return self.model.resolve_object_type(self._parent)

    @cached_property
    def is_frida_options(self) -> bool:
        return self.c_type.startswith("Frida") and self.c_type.endswith("Options")

    @cached_property
    def is_frida_list(self) -> bool:
        return self.c_type.startswith("Frida") and self.c_type.endswith("List")

    @cached_property
    def customizations(self):
        return self.model.customizations.type_customizations.get(self.name)

    @cached_property
    def constructors(self) -> List[Constructor]:
        factory = self.model.factory
        constructors = []
        custom = self.customizations
        for element in self._constructors:
            if element.get("introspectable") == "0" or element.get("deprecated") == "1":
                continue

            name = element.get("name")

            if custom is not None:
                ccust = custom.constructor
                if ccust is not None and ccust.drop:
                    continue

            (
                c_identifier,
                finish_c_identifier,
                param_list,
                has_closure_param,
                throws,
                result_element,
            ) = extract_callable_details(element, element, self, self.resolve_type)
            if has_closure_param or finish_c_identifier is not None:
                continue

            constructors.append(
                factory.constructor(
                    name, c_identifier, finish_c_identifier, param_list, throws, self
                )
            )
        return constructors

    @cached_property
    def methods(self) -> List[Method]:
        factory = self.model.factory
        methods = []
        c_prop_names = {prop.c_name for prop in self.properties}
        custom = self.customizations
        for element in self._methods:
            name = element.get("name")

            if (
                element.get("introspectable") == "0"
                or name.startswith("_")
                or name.endswith("_sync")
                or name.endswith("_finish")
            ):
                continue

            if custom is not None:
                mcust = custom.methods.get(name, None)
                if mcust is not None and mcust.drop:
                    continue

            finish_func = element.get(f"{{{GLIB_NAMESPACE}}}finish-func")
            if finish_func is None:
                finish_func = f"{name}_finish"
            result_element = next(
                (m for m in self._methods if m.get("name") == finish_func), element
            )

            (
                c_identifier,
                finish_c_identifier,
                param_list,
                has_closure_param,
                throws,
                result_element,
            ) = extract_callable_details(
                element, result_element, self, self.resolve_type
            )
            if has_closure_param:
                continue

            retval_element = result_element.find(".//return-value", GIR_NAMESPACES)
            rettype = extract_type_from_entity(retval_element, self.resolve_type)
            if rettype is not None:
                if rettype.is_frida_options:
                    continue

                nullable = retval_element.get("nullable") == "1"

                ownership_val = retval_element.get("transfer-ownership")
                transfer_ownership = (
                    TransferOwnership[ownership_val]
                    if ownership_val is not None
                    else TransferOwnership.none
                )

                retval = factory.return_value(
                    rettype, nullable, transfer_ownership, self
                )
            else:
                retval = None

            if element.get(f"{{{GLIB_NAMESPACE}}}get-property") is not None:
                is_property_accessor = True
            else:
                tokens = name.split("_", maxsplit=1)
                is_property_accessor = (
                    len(tokens) == 2
                    and tokens[0] in {"get", "set"}
                    and tokens[1] in c_prop_names
                )

            methods.append(
                factory.method(
                    name,
                    c_identifier,
                    finish_c_identifier,
                    param_list,
                    throws,
                    retval,
                    is_property_accessor,
                    self,
                )
            )
        return methods

    @cached_property
    def properties(self) -> List[Property]:
        factory = self.model.factory
        properties = []
        custom = self.customizations
        for element in self._properties:
            name = element.get("name")

            if custom is not None:
                pcust = custom.properties.get(name, None)
                if pcust is not None and pcust.drop:
                    continue

            c_name = name.replace("-", "_")
            type = extract_type_from_entity(element, self.resolve_type)
            if type.is_frida_options:
                continue
            writable = element.get("writable") == "1"
            construct_only = element.get("construct-only") == "1"

            getter = element.get("getter")
            if getter is None:
                getter = f"get_{c_name}"

            setter = element.get("setter")
            if setter is None and writable and not construct_only:
                setter = f"set_{c_name}"

            properties.append(
                factory.property_(
                    name,
                    c_name,
                    type,
                    writable,
                    construct_only,
                    getter,
                    setter,
                    self,
                )
            )
        return properties

    @cached_property
    def signals(self) -> List[Signal]:
        factory = self.model.factory
        signals = []
        custom = self.customizations
        for element in self._signals:
            name = element.get("name")

            if custom is not None:
                scust = custom.signals.get(name, None)
                if scust is not None and scust.drop:
                    continue

            c_name = name.replace("-", "_")
            param_list = extract_parameters(
                element.findall("./parameters/parameter", GIR_NAMESPACES),
                nullable_implies_optional=False,
                object_type=self,
                resolve_type=self.resolve_type,
            )
            signals.append(factory.signal(name, c_name, param_list, self))
        return signals


@dataclass
class ClassObjectType(ObjectType):
    pass


@dataclass
class InterfaceObjectType(ObjectType):
    pass


@dataclass
class Procedure:
    name: str
    c_identifier: str
    finish_c_identifier: Optional[str]
    parameters: List[Parameter]
    throws: bool

    @property
    def is_async(self) -> bool:
        return self.finish_c_identifier is not None

    @cached_property
    def input_parameters(self) -> List[Parameter]:
        return [p for p in self.parameters if p.direction != Direction.OUT]


@dataclass
class Constructor(Procedure):
    object_type: ObjectType


@dataclass
class Method(Procedure):
    return_value: Optional[ReturnValue]
    is_property_accessor: bool

    object_type: ObjectType


@dataclass
class Property:
    name: str
    c_name: str
    type: Type
    writable: bool
    construct_only: bool
    getter: Optional[str]
    setter: Optional[str]

    object_type: ObjectType


@dataclass
class Signal:
    name: str
    c_name: str
    parameters: List[Parameter]

    object_type: ObjectType


TransferOwnership = Enum("TransferOwnership", ["none", "full", "container"])


@dataclass
class Parameter:
    name: str
    type: Type
    optional: bool
    nullable: bool
    transfer_ownership: TransferOwnership
    direction: Direction

    object_type: ObjectType


@dataclass
class ReturnValue:
    type: Type
    nullable: bool
    transfer_ownership: TransferOwnership

    object_type: ObjectType


@dataclass
class Type:
    name: str
    nick: str
    c: str
    default_value: Optional[str]
    copy_func: Optional[str]
    destroy_func: Optional[str]

    @cached_property
    def is_frida_options(self) -> bool:
        return self.c.startswith("Frida") and self.c.endswith("Options *")

    @cached_property
    def from_pointer_func(self) -> Optional[str]:
        if self.name in {"gssize", "gsize", "glong", "gulong", "gint64", "guint64"}:
            return "GPOINTER_TO_SIZE"
        if self.name in {"gint", "gint8", "gint16", "gint32"}:
            return "GPOINTER_TO_INT"
        if self.name in {"gboolean", "guint", "guint8", "guint16", "guint32"}:
            return "GPOINTER_TO_UINT"
        return None

    @cached_property
    def to_pointer_func(self) -> Optional[str]:
        if self.name in {"gssize", "gsize", "glong", "gulong", "gint64", "guint64"}:
            return "GSIZE_TO_POINTER"
        if self.name in {"gint", "gint8", "gint16", "gint32"}:
            return "GINT_TO_POINTER"
        if self.name in {"gboolean", "guint", "guint8", "guint16", "guint32"}:
            return "GUINT_TO_POINTER"
        return None


class Direction(Enum):
    IN = "in"
    OUT = "out"
    INOUT = "inout"


@dataclass
class Enumeration:
    name: str
    c_type: str
    get_type: str
    _members: List[ET.Element]

    model: Optional[Model]

    @cached_property
    def members(self) -> List[EnumerationMember]:
        factory = self.model.factory
        members = []
        for element in self._members:
            c_identifier = element.get(f"{{{C_NAMESPACE}}}identifier")
            members.append(
                factory.enumeration_member(element.get("name"), c_identifier, self)
            )
        return members

    @cached_property
    def customizations(self):
        return self.model.customizations.type_customizations.get(self.name)


@dataclass
class EnumerationMember:
    name: str
    c_identifier: Optional[str]

    enumeration: Enumeration


def parse_gir(
    file_path: str, dependencies: Sequence[Model], factory: Factory
) -> Model:
    tree = ET.parse(file_path)

    el = tree.getroot().find("./namespace", GIR_NAMESPACES)
    namespace = Namespace(
        el.get("name"), el.get(f"{{{C_NAMESPACE}}}identifier-prefixes"), el
    )

    def resolve_type(name: str) -> Tuple[str, ET.Element]:
        assert (
            name not in PRIMITIVE_GIR_TYPES
        ), f"unexpectedly asked to resolve primitive type: {name}"

        tokens = name.split(".", maxsplit=1)
        if len(tokens) == 2:
            ns_name, bare_name = tokens
            if ns_name == namespace.name:
                ns = namespace
            else:
                ns = next(
                    (
                        dep.namespace
                        for dep in dependencies
                        if dep.namespace.name == ns_name
                    ),
                    None,
                )
                if ns is None:
                    assert ns is not None, f"unable to resolve namespace {ns_name}"
        else:
            ns = namespace
            bare_name = name
        qualified_name = f"{ns.name}.{bare_name}"

        element = ns.type_elements.get(bare_name)
        assert element is not None, f"unable to resolve type {bare_name}"

        return (qualified_name, element)

    object_types = OrderedDict()

    for element in namespace.element.findall("./class", GIR_NAMESPACES):
        name = element.get("name")
        c_type = element.get(f"{{{C_NAMESPACE}}}type")
        get_type = element.get(f"{{{GLIB_NAMESPACE}}}get-type")
        type_struct = element.get(f"{{{GLIB_NAMESPACE}}}type-struct")
        if type_struct is not None:
            type_struct = namespace.identifier_prefixes + type_struct
        else:
            type_struct = c_type + "Class"
        parent = element.get("parent")
        if parent is not None:
            parent, _ = resolve_type(parent)
        constructors = element.findall(".//constructor", GIR_NAMESPACES)
        methods = element.findall(".//method", GIR_NAMESPACES)
        properties = element.findall(".//property", GIR_NAMESPACES)
        signals = element.findall(".//glib:signal", GIR_NAMESPACES)
        implements = [
            e.get("name") for e in element.findall(".//implements", GIR_NAMESPACES)
        ]

        object_types[name] = factory.class_object_type(
            name=name,
            c_type=c_type,
            get_type=get_type,
            type_struct=type_struct,
            parent=parent,
            constructors=constructors,
            methods=methods,
            properties=properties,
            signals=signals,
            implements=implements,
            resolve_type=resolve_type,
            model=None,
        )

    for element in namespace.element.findall("./interface", GIR_NAMESPACES):
        name = element.get("name")
        c_type = element.get(f"{{{C_NAMESPACE}}}type")
        get_type = element.get(f"{{{GLIB_NAMESPACE}}}get-type")
        type_struct = element.get(f"{{{GLIB_NAMESPACE}}}type-struct")
        if type_struct is not None:
            type_struct = namespace.identifier_prefixes + type_struct
        else:
            type_struct = c_type + "Iface"
        prereq = element.find(".//prerequisite", GIR_NAMESPACES)
        parent = prereq.get("name") if prereq is not None else None
        if parent is not None:
            parent, _ = resolve_type(parent)
        constructors = []
        methods = element.findall(".//method", GIR_NAMESPACES)
        properties = element.findall(".//property", GIR_NAMESPACES)
        signals = element.findall(".//glib:signal", GIR_NAMESPACES)

        object_types[name] = factory.interface_object_type(
            name=name,
            c_type=c_type,
            get_type=get_type,
            type_struct=type_struct,
            parent=parent,
            constructors=constructors,
            methods=methods,
            properties=properties,
            signals=signals,
            resolve_type=resolve_type,
            model=None,
        )

    enumerations = OrderedDict()
    error_domain = None

    for element in namespace.element.findall("./enumeration", GIR_NAMESPACES):
        enum_name = element.get("name")
        enum_c_type = element.get(f"{{{C_NAMESPACE}}}type")
        get_type = element.get(f"{{{GLIB_NAMESPACE}}}get-type")
        members = element.findall(".//member", GIR_NAMESPACES)
        enumeration = factory.enumeration(
            enum_name, enum_c_type, get_type, members, None
        )
        if element.get(f"{{{GLIB_NAMESPACE}}}error-domain") is not None:
            error_domain = enumeration
            continue
        enumerations[enum_name] = enumeration

    model = factory.model(
        namespace,
        object_types,
        enumerations,
        error_domain=error_domain,
        factory=factory,
    )

    for t in object_types.values():
        t.model = model
    for t in enumerations.values():
        t.model = model

    return model


def extract_callable_details(
    element: ET.Element,
    result_element: ET.Element,
    object_type: ObjectType,
    resolve_type: ResolveTypeCallback,
) -> Tuple[str, Optional[str], List[Parameter], bool, bool, ET.Element]:
    c_identifier = element.get(f"{{{C_NAMESPACE}}}identifier")

    parameters = element.findall("./parameters/parameter", GIR_NAMESPACES)
    full_param_list = extract_parameters(
        parameters,
        nullable_implies_optional=True,
        object_type=object_type,
        resolve_type=resolve_type,
    )
    param_list = list(all_regular_parameters(full_param_list))
    has_closure_param = any((param.get("closure") == "1" for param in parameters))

    is_async = any(
        param.type.name == "Gio.AsyncReadyCallback" for param in full_param_list
    )
    if not is_async:
        result_element = element

    finish_c_identifier = (
        result_element.get(f"{{{C_NAMESPACE}}}identifier") if is_async else None
    )

    throws = result_element.get("throws") == "1"

    return (
        c_identifier,
        finish_c_identifier,
        param_list,
        has_closure_param,
        throws,
        result_element,
    )


def extract_parameters(
    parameter_elements: List[ET.Element],
    nullable_implies_optional: bool,
    object_type: ObjectType,
    resolve_type: ResolveTypeCallback,
) -> List[Parameter]:
    factory = object_type.model.factory
    entries = []
    for param in parameter_elements:
        nullable = param.get("nullable") == "1"
        entries.append((param, nullable))

    last_required_index = None
    for i, (param, nullable) in enumerate(entries):
        optional = nullable and nullable_implies_optional
        if not optional:
            last_required_index = i

    param_list = []
    for i, (param, nullable) in enumerate(entries):
        name = param.get("name")
        type = extract_type_from_entity(param, resolve_type)

        if last_required_index is None or i > last_required_index:
            optional = nullable and nullable_implies_optional
        else:
            optional = False

        ownership_val = param.get("transfer-ownership")
        transfer_ownership = (
            TransferOwnership[ownership_val]
            if ownership_val is not None
            else TransferOwnership.none
        )

        raw_direction = param.get("direction")
        direction = (
            Direction(raw_direction) if raw_direction is not None else Direction.IN
        )

        param_list.append(
            factory.parameter(
                name,
                type,
                optional,
                nullable,
                transfer_ownership,
                direction,
                object_type,
            )
        )
    return param_list


def all_regular_parameters(parameters: List[Parameter]) -> Iterator[Parameter]:
    callback_index = None
    for i, param in enumerate(parameters):
        if param.type.name == "Gio.AsyncReadyCallback":
            callback_index = i
            continue

        if callback_index is not None and i == callback_index + 1:
            continue

        yield param


def extract_type_from_entity(
    parent_element: ET.Element, resolve_type: ResolveTypeCallback
) -> Optional[Type]:
    child = parent_element.find("type", GIR_NAMESPACES)
    if child is None:
        child = parent_element.find("array", GIR_NAMESPACES)
        assert child is not None
        element_type = extract_type_from_entity(child, resolve_type)
        if element_type.name == "utf8":
            return Type(
                "utf8[]",
                "strv",
                "gchar **",
                "NULL",
                "g_strdupv",
                "g_strfreev",
            )
        elif element_type.name == "gchar":
            return Type("char[]", "chararray", "gchar *", "NULL", "NULL", "NULL")
        elif element_type.name == "GObject.Value":
            return Type("Value[]", "valuearray", "GValue *", "NULL", "NULL", "NULL")
        else:
            assert (
                element_type.name == "guint8"
            ), f"unsupported array type: {element_type.name}"
            return Type("uint8[]", "bytearray", "guint8 *", "NULL", "NULL", "NULL")

    return parse_type(child, resolve_type)


def parse_type(
    element: ET.Element, resolve_type: ResolveTypeCallback
) -> Optional[Type]:
    name = element.get("name")
    assert name is not None
    if name == "none":
        return None

    is_primitive = name in PRIMITIVE_GIR_TYPES
    c_type = element.get(f"{{{C_NAMESPACE}}}type")

    core_tag = None
    if is_primitive:
        type_element = element
        if c_type is None:
            c_type = name
    else:
        name, type_element = resolve_type(name)
        if type_element.tag.startswith(CORE_TAG_PREFIX):
            core_tag = type_element.tag[len(CORE_TAG_PREFIX) :]
        c_type = type_element.get(f"{{{C_NAMESPACE}}}type")
        if core_tag in {"class", "interface", "record"}:
            c_type += "*"

    nick = type_nick_from_name(name, element, resolve_type)
    c = c_type.replace("*", " *")

    default_value = "NULL" if "*" in c else None

    if name == "utf8":
        copy_func = "g_strdup"
        destroy_func = "g_free"
    elif name == "utf8[]":
        copy_func = "g_strdupv"
        destroy_func = "g_strfreev"
    elif name == "GLib.HashTable":
        copy_func = "g_hash_table_ref"
        destroy_func = "g_hash_table_unref"
    elif name == "GLib.Quark":
        copy_func = None
        destroy_func = None
    elif name == "GObject.Value":
        copy_func = "g_value_copy"
        destroy_func = "g_value_reset"
    elif name == "GObject.Closure":
        copy_func = "g_closure_ref"
        destroy_func = "g_closure_unref"
    elif core_tag in {"class", "interface"}:
        copy_func = "g_object_ref"
        destroy_func = "g_object_unref"
    elif is_primitive or core_tag in {"bitfield", "callback", "enumeration"}:
        copy_func = None
        destroy_func = None
    else:
        copy_func = type_element.get("copy-function")
        destroy_func = type_element.get("free-function")
        assert (
            destroy_func is not None
        ), f"unable to resolve destroy function for {name}, core_tag={core_tag}"

    return Type(name, nick, c, default_value, copy_func, destroy_func)


def type_nick_from_name(
    name: str, element: ET.Element, resolve_type: ResolveTypeCallback
) -> str:
    if name == "GLib.PollFD":
        return "pollfd"

    tokens = name.split(".", maxsplit=1)
    if len(tokens) == 1:
        result = tokens[0]
        if result.startswith("g"):
            result = result[1:]
    else:
        result = to_snake_case(tokens[1])

    if result == "hash_table":
        key_type = parse_type(element[0], resolve_type)
        value_type = parse_type(element[1], resolve_type)
        assert (
            key_type.name == "utf8" and value_type.name == "GLib.Variant"
        ), "only GHashTable<string, Variant> is supported for now"
        result = "vardict"

    return result
