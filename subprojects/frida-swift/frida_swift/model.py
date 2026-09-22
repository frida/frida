from __future__ import annotations

from collections import OrderedDict, defaultdict
from dataclasses import dataclass, field
from functools import cached_property
from typing import List, Mapping, Optional, Sequence

import frida_bindgen_core as core
from frida_bindgen_core.model import GIR_NAMESPACES
from frida_bindgen_core.naming import to_camel_case, to_snake_case

# Swift keywords that cannot be used bare as identifiers.
SWIFT_KEYWORDS = {
    "protocol", "class", "struct", "enum", "func", "var", "let", "default",
    "for", "in", "is", "as", "where", "case", "switch", "return", "throws",
    "try", "self", "Self", "Type", "Any", "public", "private", "internal",
    "static", "import", "extension", "init", "deinit", "operator", "guard",
    "repeat", "while", "do", "catch", "throw", "defer", "async", "await",
}


def swift_ident(name: str) -> str:
    ident = to_camel_case(name)
    if ident in SWIFT_KEYWORDS:
        return f"`{ident}`"
    return ident


# The special-cased "detail" record types renamed in the Swift surface.
DETAIL_RENAMES = {
    "Application": "ApplicationDetails",
    "Process": "ProcessDetails",
    "Spawn": "SpawnDetails",
    "Child": "ChildDetails",
    "Crash": "CrashDetails",
}


class Model(core.Model):
    @cached_property
    def regular_object_types(self) -> List["ObjectType"]:
        return [
            t
            for t in self.object_types.values()
            if not t.is_frida_list and not t.is_frida_options and t.emit_class
        ]

    @cached_property
    def interface_impls(self) -> List["InterfaceObjectType"]:
        return [
            t
            for t in self.object_types.values()
            if isinstance(t, InterfaceObjectType) and t.is_implementable
        ]

    @cached_property
    def enumerations_kept(self) -> List["Enumeration"]:
        return [e for e in self.enumerations.values() if not e.drop]

    def swift_class_for(self, name: str) -> Optional["ObjectType"]:
        bare = name.split(".", maxsplit=1)[-1]
        return self.object_types.get(bare)


class ObjectType(core.ObjectType):
    @property
    def emit(self) -> bool:
        """The type is part of the Swift surface (referenceable/wrappable)."""
        custom = self.customizations
        if custom is not None and custom.drop:
            return False
        # Skip GObject base and the Gio helper classes: those live in
        # separately compiled Swift modules (GLib) and are not part of the
        # generated Frida surface.
        return self.c_type.startswith("Frida")

    @property
    def as_asset(self) -> bool:
        """Kept as a hand-written asset (bespoke signals/RPC); not generated."""
        custom = self.customizations
        return custom is not None and getattr(custom, "as_asset", False)

    @property
    def emit_class(self) -> bool:
        """We emit a generated class definition for this type."""
        return self.emit and not self.as_asset

    @property
    def generate_signals(self) -> bool:
        custom = self.customizations
        return custom is not None and getattr(custom, "generate_signals", False)

    @property
    def extra_conformances(self) -> List[str]:
        custom = self.customizations
        return custom.conformances if custom is not None else []

    @property
    def extra_members(self) -> Optional[str]:
        custom = self.customizations
        return custom.extra_members if custom is not None else None

    @property
    def sendable(self) -> bool:
        custom = self.customizations
        return custom is not None and custom.sendable

    @property
    def custom_members(self) -> Optional[str]:
        custom = self.customizations
        return custom.custom_members if custom is not None else None

    @property
    def custom_module(self) -> Optional[str]:
        custom = self.customizations
        return custom.custom_module if custom is not None else None

    @property
    def custom_init(self) -> Optional[str]:
        custom = self.customizations
        return custom.custom_init if custom is not None else None

    @property
    def custom_deinit(self) -> Optional[str]:
        custom = self.customizations
        return custom.custom_deinit if custom is not None else None

    @property
    def event_cases(self) -> List[str]:
        custom = self.customizations
        return custom.event_cases if custom is not None else []

    @cached_property
    def emitted_signals(self) -> List["Signal"]:
        return [s for s in self.signals if not s.dropped]

    @cached_property
    def swift_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.swift_name is not None:
            return custom.swift_name
        return DETAIL_RENAMES.get(self.name, self.name)

    @cached_property
    def c_symbol_prefix(self) -> str:
        return "frida_" + to_snake_case(self.name)

    @cached_property
    def emitted_parent(self) -> Optional["ObjectType"]:
        parent = self.parent
        if parent is None or not parent.emit_class:
            return None
        return parent

    @cached_property
    def has_emitted_subclasses(self) -> bool:
        return any(t.emitted_parent is self for t in self.model.regular_object_types)

    @cached_property
    def inherited_member_names(self) -> set:
        names = set()
        current = self.emitted_parent
        while current is not None:
            names.update(m.property_name for m in current.emitted_properties)
            names.update(m.swift_name for m in current.emitted_async_methods)
            names.update(m.swift_name for m in current.emitted_custom_methods)
            names.update(m.swift_name for m in current.emitted_sync_methods)
            names.update(current.inherited_member_names)
            current = current.emitted_parent
        return names

    @cached_property
    def emitted_properties(self) -> List["Method"]:
        return [m for m in self.methods if m.is_swift_getter]

    @cached_property
    def emitted_async_methods(self) -> List["Method"]:
        return [m for m in self.methods if m.is_swift_async]

    @cached_property
    def emitted_custom_methods(self) -> List["Method"]:
        return [m for m in self.methods if m.is_swift_custom]

    @cached_property
    def emitted_sync_methods(self) -> List["Method"]:
        return [m for m in self.methods if m.is_swift_sync]


    @property
    def is_interface(self) -> bool:
        return False


class ClassObjectType(ObjectType):
    pass


class InterfaceObjectType(ObjectType):
    @property
    def is_interface(self) -> bool:
        return True

    @property
    def emit(self) -> bool:
        # The interface is a referenceable Swift type (its generated base
        # class), as long as it is a user-implementable Frida interface.
        return self.is_implementable

    @property
    def emit_class(self) -> bool:
        # Interfaces are emitted via the interface-implementation path, not as
        # ordinary wrapper classes.
        return False

    @cached_property
    def is_implementable(self) -> bool:
        if self.customizations is not None and self.customizations.drop:
            return False
        return self.c_type.startswith("Frida") and any(
            m.is_async for m in self.methods
        )

    @cached_property
    def implemented_method(self) -> "Method":
        return next(m for m in self.methods if m.is_async)


class Constructor(core.Constructor):
    pass


class Method(core.Method):
    @cached_property
    def customizations(self) -> Optional["MethodCustomizations"]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.methods.get(self.name)

    @property
    def dropped(self) -> bool:
        c = self.customizations
        return c is not None and c.drop

    @property
    def force_property(self) -> bool:
        c = self.customizations
        return c is not None and c.as_property

    @property
    def custom_logic(self) -> Optional[str]:
        c = self.customizations
        return c.custom_logic if c is not None else None

    @property
    def param_typings(self) -> Optional[List[str]]:
        c = self.customizations
        return c.param_typings if c is not None else None

    @property
    def custom_body(self) -> Optional[str]:
        c = self.customizations
        return c.custom_body if c is not None else None

    @property
    def is_swift_custom(self) -> bool:
        return not self.dropped and self.custom_logic is not None

    @cached_property
    def is_swift_sync(self) -> bool:
        if self.dropped or self.is_async or self.is_swift_getter or self.is_swift_custom:
            return False
        if self.is_property_accessor or self.force_property or self.name.startswith("set_"):
            return False
        model = self.object_type.model
        rv = self.return_value
        if rv is not None:
            kind = swift_return_kind(rv.type, model)
            if kind is None or kind[0] not in ("void", "strv"):
                return False
        return all(swift_input_kind(p.type, model) is not None for p in self.swift_input_parameters)

    @cached_property
    def swift_name(self) -> str:
        c = self.customizations
        if c is not None and c.swift_name is not None:
            return c.swift_name
        return swift_ident(self.name)

    @cached_property
    def property_name(self) -> str:
        c = self.customizations
        if c is not None and c.property_name is not None:
            return c.property_name
        name = self.name
        for prefix in ("get_",):
            if name.startswith(prefix):
                name = name[len(prefix):]
                break
        return swift_ident(name)

    @cached_property
    def is_swift_getter(self) -> bool:
        if self.dropped or self.is_async or self.throws:
            return False
        if not (self.is_property_accessor or self.force_property):
            return False
        if self.name.startswith("set_"):
            return False
        if self.input_parameters:
            return False
        if self.return_value is None:
            return False
        return swift_return_kind(self.return_value.type, self.object_type.model) is not None

    @cached_property
    def is_swift_async(self) -> bool:
        if self.dropped or not self.is_async:
            return False
        model = self.object_type.model
        if swift_return_kind_async(self.return_value.type if self.return_value else None, model) is None:
            return False
        for p in self.input_parameters:
            if p.type.name == "Gio.Cancellable":
                continue
            if p.type.is_frida_options:
                continue
            if swift_input_kind(p.type, model) is None:
                return False
        return True

    @cached_property
    def swift_input_parameters(self) -> List["Parameter"]:
        return [p for p in self.input_parameters if p.type.name != "Gio.Cancellable"]


class Property(core.Property):
    pass


class Signal(core.Signal):
    @cached_property
    def customizations(self) -> Optional["SignalCustomizations"]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.signals.get(self.name)

    @property
    def dropped(self) -> bool:
        c = self.customizations
        return c is not None and c.drop

    @property
    def terminal(self) -> bool:
        c = self.customizations
        return c is not None and c.terminal

    @cached_property
    def event_name(self) -> str:
        c = self.customizations
        if c is not None and c.event_name is not None:
            return c.event_name
        return to_camel_case(self.c_name)

    @property
    def transform(self) -> Mapping[int, tuple]:
        c = self.customizations
        return c.transform if c is not None and c.transform is not None else {}

    @cached_property
    def event_payload(self):
        """List of (label_or_None, Parameter) in event-declaration order."""
        c = self.customizations
        if c is not None and c.payload is not None:
            by_name = {p.name: p for p in self.parameters}
            return [(label, by_name[pname]) for (label, pname) in c.payload]
        params = self.parameters
        if len(params) == 1:
            return [(None, params[0])]
        return [(p.swift_name, p) for p in params]


class Parameter(core.Parameter):
    @cached_property
    def swift_name(self) -> str:
        return swift_ident(self.name)


class ReturnValue(core.ReturnValue):
    pass


class Enumeration(core.Enumeration):
    @cached_property
    def customizations(self) -> Optional["EnumerationCustomizations"]:
        c = self.model.customizations
        if c is None:
            return None
        return c.type_customizations.get(self.name)

    @property
    def drop(self) -> bool:
        c = self.customizations
        return c is not None and c.drop

    @cached_property
    def swift_name(self) -> str:
        return self.name

    @cached_property
    def first_value(self) -> int:
        c = self.customizations
        if c is not None and c.first_value is not None:
            return c.first_value
        return 0


class EnumerationMember(core.EnumerationMember):
    @cached_property
    def customizations(self) -> Optional["EnumerationMemberCustomizations"]:
        c = self.enumeration.customizations
        if c is None:
            return None
        return c.members.get(self.name)

    @cached_property
    def swift_name(self) -> str:
        c = self.customizations
        if c is not None and c.swift_name is not None:
            return c.swift_name
        return swift_ident(self.name)


# --- Swift type mapping (consumer-side, reads existing core Type fields) ---

_SCALAR_MAP = {
    "gboolean": "Bool",
    "gint": "Int", "gint8": "Int", "gint16": "Int", "gint32": "Int",
    "gint64": "Int64", "glong": "Int", "gssize": "Int",
    "guint": "UInt", "guint8": "UInt", "guint16": "UInt", "guint32": "UInt",
    "guint64": "UInt64", "gulong": "UInt", "gsize": "Int",
    "gfloat": "Float", "gdouble": "Double",
}


def swift_input_kind(type: core.Type, model):
    """Classify an input parameter type; None if unsupported."""
    n = type.name
    if n == "utf8":
        return ("string", "String")
    if n == "GLib.Bytes":
        return ("bytes", "[UInt8]")
    if n == "GLib.Variant":
        return ("variant", "Any")
    if n in _SCALAR_MAP:
        return ("scalar", _SCALAR_MAP[n])
    enum = model.enumerations.get(n.split(".")[-1])
    if enum is not None and not enum.drop:
        return ("enum", enum.swift_name)
    obj = model.swift_class_for(n)
    if obj is not None and obj.emit and not obj.is_frida_list and not obj.is_interface:
        return ("object", obj.swift_name)
    return None


def swift_return_kind(type: Optional[core.Type], model):
    """Classify a sync-accessor return type; None if unsupported."""
    if type is None:
        return ("void", "Void")
    n = type.name
    if n == "utf8":
        return ("string", "String")
    if n == "utf8[]":
        return ("strv", "[String]")
    if n == "GLib.Bytes":
        return ("bytes", "[UInt8]")
    if n == "GLib.HashTable":
        return ("vardict", "[String: Any]")
    if n in _SCALAR_MAP:
        return ("scalar", _SCALAR_MAP[n])
    enum = model.enumerations.get(n.split(".")[-1])
    if enum is not None and not enum.drop:
        return ("enum", enum.swift_name)
    obj = model.swift_class_for(n)
    if obj is not None and obj.emit and not obj.is_frida_list and not obj.is_interface:
        return ("object", obj.swift_name)
    return None


def swift_return_kind_async(type: Optional[core.Type], model):
    """Classify an async return; supports everything sync does plus lists."""
    if type is None:
        return ("void", "Void")
    n = type.name
    if n == "Gio.IOStream":
        return ("giostream", "GLib.IOStream")
    if n == "GLib.Variant":
        return ("variant", "Any")
    obj = model.swift_class_for(n)
    if obj is not None and obj.is_frida_list:
        element = list_element_type(obj, model)
        if element is not None and element.emit:
            return ("list", f"[{element.swift_name}]")
        return None
    return swift_return_kind(type, model)


def list_element_type(list_obj, model):
    """Resolve the element ObjectType of a Frida*List via its get() method."""
    for m in list_obj.methods:
        if m.name == "get" and m.return_value is not None:
            return model.swift_class_for(m.return_value.type.name)
    return None


def options_object_type(type: core.Type, model):
    """Resolve the ObjectType backing a Frida*Options parameter."""
    return model.swift_class_for(type.name)


# --- Customization dataclasses (data-driven, mirrors the Python consumer) ---


@dataclass
class Customizations:
    type_customizations: Mapping[str, "TypeCustomizations"] = field(
        default_factory=OrderedDict
    )


@dataclass
class TypeCustomizations:
    drop: bool = False


@dataclass
class ObjectTypeCustomizations(TypeCustomizations):
    swift_name: Optional[str] = None
    as_asset: bool = False
    generate_signals: bool = False
    sendable: bool = False
    conformances: List[str] = field(default_factory=list)
    extra_members: Optional[str] = None
    # Asset-file snippet names (relative to frida_swift/assets), injected into
    # the generated class body / module / init / deinit, or replacing the init.
    custom_members: Optional[str] = None
    custom_module: Optional[str] = None
    custom_init: Optional[str] = None
    custom_deinit: Optional[str] = None
    event_cases: List[str] = field(default_factory=list)
    constructor: Optional["MethodCustomizations"] = None
    methods: Mapping[str, "MethodCustomizations"] = field(
        default_factory=lambda: defaultdict(dict)
    )
    properties: Mapping[str, "PropertyCustomizations"] = field(
        default_factory=lambda: defaultdict(dict)
    )
    signals: Mapping[str, "SignalCustomizations"] = field(
        default_factory=lambda: defaultdict(dict)
    )


@dataclass
class MethodCustomizations:
    drop: bool = False
    as_property: bool = False
    swift_name: Optional[str] = None
    property_name: Optional[str] = None
    # For strv getters/option-setters: override the exposed Swift type and the
    # Marshal converter used to bridge it (e.g. dict<->KEY=VALUE strv).
    swift_type: Optional[str] = None
    strv_converter: Optional[str] = None
    # Emit a plain (non-async) method whose facade signature is `param_typings`
    # and whose body is `custom_logic` followed by the generated C call; the call
    # passes the C-named locals custom_logic defines (e.g. `let json = ...`) and
    # marshals the rest. Mirrors frida-node's declarative post/narrowcast.
    param_typings: Optional[List[str]] = None
    custom_logic: Optional[str] = None
    # Emit the named asset in place of the generated method, keeping the method
    # where the generated one would have been.
    custom_body: Optional[str] = None


@dataclass
class PropertyCustomizations:
    drop: bool = False


@dataclass
class SignalCustomizations:
    drop: bool = False
    event_name: Optional[str] = None
    terminal: bool = False
    # Optional [(label_or_None, param_name), ...] to reorder/relabel payload.
    payload: Optional[List] = None
    # Optional {param_index: (swift_type, converter)} to declaratively transform
    # a payload arg (e.g. JSON-parse a string) in the generated handler, instead
    # of hand-writing it. The converter is applied (throwing, guarded) to the
    # base-marshalled value; the enum case exposes swift_type.
    transform: Optional[Mapping[int, tuple]] = None


@dataclass
class EnumerationCustomizations(TypeCustomizations):
    first_value: Optional[int] = None
    members: Mapping[str, "EnumerationMemberCustomizations"] = field(
        default_factory=lambda: defaultdict(dict)
    )


@dataclass
class EnumerationMemberCustomizations:
    swift_name: Optional[str] = None


def _make_class(**kw):
    return ClassObjectType(
        kw["name"], kw["c_type"], kw["get_type"], kw["type_struct"],
        kw["parent"], kw["constructors"], kw["methods"], kw["properties"],
        kw["signals"], kw["resolve_type"], kw["model"],
    )


def _make_interface(**kw):
    return InterfaceObjectType(
        kw["name"], kw["c_type"], kw["get_type"], kw["type_struct"],
        kw["parent"], kw["constructors"], kw["methods"], kw["properties"],
        kw["signals"], kw["resolve_type"], kw["model"],
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


def parse_gir(file_path: str, dependencies: Sequence[Model]) -> Model:
    return core.parse_gir(file_path, dependencies, FACTORY)
