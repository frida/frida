from __future__ import annotations

from collections import OrderedDict
from dataclasses import dataclass, field
from functools import cached_property
from typing import List, Mapping, Optional, Sequence, Tuple, Union

import frida_bindgen_core as core
from frida_bindgen_core import TransferOwnership
from frida_bindgen_core.naming import to_pascal_case, to_snake_case


class Model(core.Model):
    @cached_property
    def regular_object_types(self) -> List[ObjectType]:
        return [t for t in self.object_types.values() if not t.is_frida_list]


class ObjectType(core.ObjectType):
    @cached_property
    def py_name(self) -> str:
        custom = self.customizations
        if custom is not None and custom.py_name is not None:
            return custom.py_name
        return "GObject" if self.name == "Object" else self.name

    @property
    def custom_code(self) -> Optional[CustomCode]:
        custom = self.customizations
        return custom.custom_code if custom is not None else None

    @property
    def provides_signals(self) -> bool:
        custom = self.customizations
        return custom is not None and custom.provides_signals

    @property
    def custom_constructor(self) -> Optional[str]:
        custom = self.customizations
        return custom.custom_constructor if custom is not None else None

    @property
    def constructor_custom_params(self) -> List[str]:
        custom = self.customizations
        if custom is None or custom.constructor is None:
            return []
        return custom.constructor.param_typings or []

    @property
    def constructor_custom_logic(self) -> Optional[str]:
        custom = self.customizations
        if custom is None or custom.constructor is None:
            return None
        return custom.constructor.custom_logic

    @cached_property
    def c_symbol_prefix(self) -> str:
        return f"Py{self.py_name}"

    @cached_property
    def parent_c_symbol_prefix(self) -> str:
        parent = self.parent
        return parent.c_symbol_prefix if parent is not None else "PyGObject"


class ClassObjectType(ObjectType):
    pass


class InterfaceObjectType(ObjectType):
    @cached_property
    def has_abstract_base(self) -> bool:
        custom = self.customizations
        if custom is None:
            return True
        return not custom.drop_abstract_base


class Constructor(core.Constructor):
    pass


class Method(core.Method):
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
    def operation_type_name(self) -> str:
        return f"Py{self.object_type.name}{to_pascal_case(self.name)}Operation"

    @cached_property
    def is_select_method(self) -> bool:
        return self.name.startswith("select_") or self.name.startswith("add_")

    @cached_property
    def select_plural_noun(self) -> str:
        return f"{self.name.split('_', maxsplit=1)[1]}s"

    @cached_property
    def customizations(self) -> Optional[MethodCustomizations]:
        custom = self.object_type.customizations
        if custom is None:
            return None
        return custom.methods.get(self.name)

    @property
    def suppress_facade(self) -> bool:
        custom = self.customizations
        return custom is not None and custom.suppress_facade

    @property
    def as_property(self) -> bool:
        custom = self.customizations
        return custom is not None and custom.as_property

    @property
    def custom_facade_params(self) -> List[str]:
        custom = self.customizations
        if custom is None:
            return []
        return custom.param_typings or []

    @property
    def custom_logic(self) -> Optional[Union[str, Tuple[str, str]]]:
        custom = self.customizations
        return custom.custom_logic if custom is not None else None


class Property(core.Property):
    pass


class Signal(core.Signal):
    pass


class Parameter(core.Parameter):
    @cached_property
    def ctyping(self) -> str:
        return f"{self.type.c} {self.name}"


class ReturnValue(core.ReturnValue):
    @cached_property
    def destroy_func(self) -> Optional[str]:
        if self.transfer_ownership == TransferOwnership.none:
            return None
        return self.type.destroy_func


class Enumeration(core.Enumeration):
    @cached_property
    def c_symbol_prefix(self) -> str:
        return f"Py{self.name}"


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
    type_customizations: Mapping[str, TypeCustomizations] = field(default_factory=OrderedDict)
    facade_preludes: Tuple[str, ...] = ()
    facade_epilogues: Tuple[str, ...] = ()


@dataclass
class TypeCustomizations:
    pass


@dataclass
class ModuleFunction:
    py_name: str
    c_symbol: str
    asset: str


@dataclass
class CustomCode:
    members: Optional[Tuple[str, str]] = None
    helpers: Optional[Tuple[str, str]] = None
    module_functions: Optional[Tuple[ModuleFunction, ...]] = None


@dataclass
class ObjectTypeCustomizations(TypeCustomizations):
    py_name: Optional[str] = None
    drop: bool = False
    drop_abstract_base: bool = False
    custom_code: Optional[CustomCode] = None
    provides_signals: bool = False
    custom_constructor: Optional[str] = None
    constructor: Optional[ConstructorCustomizations] = None
    methods: Mapping[str, MethodCustomizations] = field(default_factory=dict)
    properties: Mapping[str, PropertyCustomizations] = field(default_factory=dict)
    signals: Mapping[str, SignalCustomizations] = field(default_factory=dict)


@dataclass
class ConstructorCustomizations:
    drop: bool = False
    param_typings: Optional[List[str]] = None
    custom_logic: Optional[str] = None


@dataclass
class MethodCustomizations:
    drop: bool = False
    suppress_facade: bool = False
    as_property: bool = False
    param_typings: Optional[List[str]] = None
    custom_logic: Optional[Union[str, Tuple[str, str]]] = None


@dataclass
class PropertyCustomizations:
    drop: bool = False


@dataclass
class SignalCustomizations:
    drop: bool = False


@dataclass
class EnumerationCustomizations(TypeCustomizations):
    members: Mapping[str, EnumerationMemberCustomizations] = field(default_factory=dict)


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


def parse_gir(file_path: str, dependencies: Sequence[Model]) -> Model:
    return core.parse_gir(file_path, dependencies, FACTORY)
