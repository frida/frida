# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

from abc import ABCMeta, abstractmethod
from pathlib import Path
import re
import typing as T

from .model import (
    NamedObject,
    FetureCheck,
    ArgBase,
    PosArg,
    DataTypeInfo,
    Type,
    Function,
    Method,
    Object,
    ObjectType,
    ReferenceManual,
)

from mesonbuild import mlog

class _Resolver:
    def __init__(self) -> None:
        self.type_map: T.Dict[str, Object] = {}
        self.func_map: T.Dict[str, T.Union[Function, Method]] = {}
        self.processed_funcs: T.Set[str] = set()

    def _validate_named_object(self, obj: NamedObject) -> None:
        name_regex = re.compile(r'[a-zA-Z0-9_]+')
        obj.name = obj.name.strip()
        obj.description = obj.description.strip()
        assert obj.name and obj.description, 'Both name and description must be set'
        assert obj.name.islower(), f'Object names must be lower case ({obj.name})'
        assert name_regex.match(obj.name) or obj.name == '[index]', f'Invalid name {obj.name}'

    def _validate_feature_check(self, obj: FetureCheck) -> None:
        meson_version_reg = re.compile(r'[0-9]+\.[0-9]+\.[0-9]+')
        obj.since = obj.since.strip()
        obj.deprecated = obj.deprecated.strip()
        if obj.since:
            assert meson_version_reg.match(obj.since)
        if obj.deprecated:
            assert meson_version_reg.match(obj.deprecated)

    def _resolve_type(self, raw: str) -> Type:
        typ = Type(raw)
        # We can't use `types = raw.split('|')`, because of `list[str | env]`
        types: T.List[str] = ['']
        stack = 0
        for c in raw:
            if stack == 0 and c == '|':
                types += ['']
                continue
            if c == '[':
                stack += 1
            if c == ']':
                stack -= 1
            types[-1] += c
        types = [x.strip() for x in types]
        for t in types:
            t = t.strip()
            idx = t.find('[')
            base_type = t
            held_type = None
            if idx > 0:
                base_type = t[:idx]
                held_type = self._resolve_type(t[idx+1:-1])
            assert base_type in self.type_map, f'No known object {t}'
            obj = self.type_map[base_type]
            typ.resolved += [DataTypeInfo(obj, held_type)]
        return typ

    def _validate_func(self, func: T.Union[Function, Method]) -> None:
        # Always run basic checks, since they also slightly post-process (strip) some strings
        self._validate_named_object(func)
        self._validate_feature_check(func)

        func_id = f'{func.obj.name}.{func.name}' if isinstance(func, Method) else func.name
        if func_id in self.processed_funcs:
            return

        func.returns = self._resolve_type(func.returns.raw)

        all_args: T.List[ArgBase] = []
        all_args += func.posargs
        all_args += func.optargs
        all_args += func.kwargs.values()
        all_args += [func.varargs] if func.varargs else []

        for arg in all_args:
            arg.type = self._resolve_type(arg.type.raw)

        # Handle returned_by
        for obj in func.returns.resolved:
            obj.data_type.returned_by += [func]

        # Handle kwargs inheritance
        for base_name in func.kwargs_inherit:
            base_name = base_name.strip()
            assert base_name in self.func_map, f'Unknown base function `{base_name}` for {func.name}'
            base = self.func_map[base_name]
            if base_name not in self.processed_funcs:
                self._validate_func(base)

            curr_keys = set(func.kwargs.keys())
            base_keys = set(base.kwargs.keys())

            # Calculate the missing kwargs from the current set
            missing = {k: v for k, v in base.kwargs.items() if k in base_keys - curr_keys}
            func.kwargs.update(missing)

        # Handle other args inheritance
        _T = T.TypeVar('_T', bound=T.Union[ArgBase, T.List[PosArg]])
        def resolve_inherit(name: str, curr: _T, resolver: T.Callable[[Function], _T]) -> _T:
            if name and not curr:
                name = name.strip()
                assert name in self.func_map, f'Unknown base function `{name}` for {func.name}'
                if name not in self.processed_funcs:
                    self._validate_func(self.func_map[name])
                ref_args = resolver(self.func_map[name])
                assert ref_args is not None, f'Inherited function `{name}` does not have inherited args set'
                return ref_args
            return curr

        func.posargs = resolve_inherit(func.posargs_inherit, func.posargs, lambda x: x.posargs)
        func.optargs = resolve_inherit(func.optargs_inherit, func.optargs, lambda x: x.optargs)
        func.varargs = resolve_inherit(func.varargs_inherit, func.varargs, lambda x: x.varargs)

        self.processed_funcs.add(func_id)

    def validate_and_resolve(self, manual: ReferenceManual) -> ReferenceManual:
        mlog.log('Validating loaded manual...')

        # build type map and func map for methods
        for obj in manual.objects:
            assert obj.name not in self.type_map, f'Duplicate object name {obj.name}'
            self.type_map[obj.name] = obj
            for m in obj.methods:
                mid = f'{obj.name}.{m.name}'
                assert mid not in self.type_map, f'Duplicate method {mid}'
                self.func_map[mid] = m

        # Build func map for functions
        for func in manual.functions:
            assert func.name not in [*self.func_map.keys()], f'Duplicate function {func.name}'
            self.func_map[func.name] = func

        mlog.log('Validating functions...')
        for func in manual.functions:
            mlog.log(' -- validating', mlog.bold(func.name))
            self._validate_func(func)

        mlog.log('Validating objects...')
        for obj in manual.objects:
            mlog.log(' -- validating', mlog.bold(obj.name))
            self._validate_named_object(obj)
            self._validate_feature_check(obj)
            # Resolve and validate inheritance
            if obj.extends:
                assert obj.extends in self.type_map, f'Unknown extends object {obj.extends} in {obj.name}'
                obj.extends_obj = self.type_map[obj.extends]
                obj.extends_obj.extended_by += [obj]
            # Only returned objects can be associated with module
            if obj.obj_type is not ObjectType.RETURNED:
                assert obj.defined_by_module is None
            for m in obj.methods:
                assert m.obj is obj
                self._validate_func(m)

        # Resolve inherited methods
        for obj in manual.objects:
            inherited_methods = obj.inherited_methods
            curr = obj.extends_obj
            while curr is not None:
                inherited_methods += curr.methods
                curr = curr.extends_obj
        return manual

class LoaderBase(metaclass=ABCMeta):
    def __init__(self) -> None:
        self._input_files: T.List[Path] = []

    @property
    def input_files(self) -> T.List[Path]:
        return list(self._input_files)

    def read_file(self, f: Path) -> str:
        assert f.exists()
        assert f.is_file()
        self._input_files += [f.resolve()]
        return f.read_text(encoding='utf-8')

    @abstractmethod
    def load_impl(self) -> ReferenceManual:
        pass

    def load(self) -> ReferenceManual:
        self._input_files = []  # Reset input files
        manual = self.load_impl()
        resolver = _Resolver()
        with mlog.nested():
            return resolver.validate_and_resolve(manual)
