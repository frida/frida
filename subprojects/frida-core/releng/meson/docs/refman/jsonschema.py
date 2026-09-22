# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

import typing as T

# The following variables define the current version of
# the JSON documentation format. This is different from
# the Meson version

VERSION_MAJOR = 1  # Changes here indicate breaking format changes (changes to existing keys)
VERSION_MINOR = 1  # Changes here indicate non-breaking changes (only new keys are added to the existing structure)

if T.TYPE_CHECKING:
    from typing_extensions import TypedDict

    class BaseObject(TypedDict):
        '''
            Base object for most dicts in the JSON doc.

            All objects inheriting from BaseObject will support
            the keys specified here:
        '''
        name:        str
        description: str
        since:       T.Optional[str]
        deprecated:  T.Optional[str]
        notes:       T.List[str]
        warnings:    T.List[str]

    class Type(TypedDict):
        obj:   str                 # References an object from `root.objects`
        holds: T.Sequence[object]  # Mypy does not support recursive dicts, but this should be T.List[Type]...

    class Argument(BaseObject):
        '''
            Object that represents any type of a single function or method argument.
        '''
        type:        T.List[Type]  # A non-empty list of types that are supported.
        type_str:    str           # Formatted version of `type`. Is guaranteed to not contain any whitespaces.
        required:    bool
        default:     T.Optional[str]
        min_varargs: T.Optional[int]  # Only relevant for varargs, must be `null` for all other types of arguments
        max_varargs: T.Optional[int]  # Only relevant for varargs, must be `null` for all other types of arguments

    class Function(BaseObject):
        '''
            Represents a function or method.
        '''
        returns:        T.List[Type]  # A non-empty list of types that are supported.
        returns_str:    str           # Formatted version of `returns`. Is guaranteed to not contain any whitespaces.
        example:        T.Optional[str]
        posargs:        T.Dict[str, Argument]
        optargs:        T.Dict[str, Argument]
        kwargs:         T.Dict[str, Argument]
        varargs:        T.Optional[Argument]
        arg_flattening: bool

    class Object(BaseObject):
        '''
            Represents all types of Meson objects. The specific object type is stored in the `object_type` field.
        '''
        example:           T.Optional[str]
        object_type:       str                    # Defines the object type: Must be one of: ELEMENTARY, BUILTIN, MODULE, RETURNED
        methods:           T.Dict[str, Function]
        is_container:      bool
        extends:           T.Optional[str]
        returned_by:       T.List[str]
        extended_by:       T.List[str]
        defined_by_module: T.Optional[str]

    class ObjectsByType(TypedDict):
        '''
            References to other objects are stored here for ease of navigation / filtering
        '''
        elementary: T.List[str]
        builtins:   T.List[str]
        returned:   T.List[str]
        modules:    T.Dict[str, T.List[str]]



    class Root(TypedDict):
        '''
            The root object of the JSON reference manual
        '''
        version_major:   int # See the description above for
        version_minor:   int # VERSION_MAJOR and VERSION_MINOR
        meson_version:   str
        functions:       T.Dict[str, Function]  # A mapping of <name> to a `Function` object for *all* Meson functions
        objects:         T.Dict[str, Object]    # A mapping of <name> to a `Object`   object for *all* Meson objects (including modules, elementary, etc.)
        objects_by_type: ObjectsByType
