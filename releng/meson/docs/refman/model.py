# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

from dataclasses import dataclass, field
from enum import Enum
import typing as T

# Utils
@dataclass
class NamedObject:
    name: str
    description: str

    @property
    def hidden(self) -> bool:
        return self.name.startswith('_')

@dataclass
class FetureCheck:
    since: str
    deprecated: str

@dataclass
class DataTypeInfo:
    data_type: 'Object'
    holds: T.Optional['Type']

@dataclass
class Type:
    raw: str
    resolved: T.List[DataTypeInfo] = field(init=False, default_factory=list)


# Arguments
@dataclass
class ArgBase(NamedObject, FetureCheck):
    type: Type

@dataclass
class PosArg(ArgBase):
    default: str

@dataclass
class VarArgs(ArgBase):
    min_varargs: int
    max_varargs: int

@dataclass
class Kwarg(ArgBase):
    required: bool
    default: str


# Function
@dataclass
class Function(NamedObject, FetureCheck):
    notes: T.List[str]
    warnings: T.List[str]
    returns: Type
    example: str
    posargs: T.List[PosArg]
    optargs: T.List[PosArg]
    varargs: T.Optional[VarArgs]
    kwargs: T.Dict[str, Kwarg]
    posargs_inherit: str
    optargs_inherit: str
    varargs_inherit: str
    kwargs_inherit: T.List[str]
    arg_flattening: bool

@dataclass
class Method(Function):
    obj: 'Object'


# Types and objects
class ObjectType(Enum):
    ELEMENTARY = 0
    BUILTIN = 1
    MODULE = 2
    RETURNED = 3

@dataclass
class Object(NamedObject, FetureCheck):
    notes: T.List[str]
    warnings: T.List[str]
    long_name: str
    example: str
    obj_type: ObjectType
    methods: T.List[Method]
    is_container: bool
    extends: str
    extends_obj: T.Optional['Object'] = None
    defined_by_module: T.Optional['Object'] = None
    returned_by: T.List[T.Union[Function, Method]] = field(default_factory=list)
    extended_by: T.List['Object'] = field(default_factory=list)
    inherited_methods: T.List[Method] = field(default_factory=list)

# ROOT
@dataclass
class ReferenceManual:
    functions: T.List[Function]
    objects: T.List[Object]
