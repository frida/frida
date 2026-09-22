# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

from .generatorbase import GeneratorBase
from .model import ReferenceManual, Object, Function, DataTypeInfo, Type, ObjectType

from mesonbuild import mlog
import typing as T

def my_nested() -> T.ContextManager[None]:
    prefix = '|' * mlog.get_log_depth()
    return mlog.nested(prefix)

class GeneratorPrint(GeneratorBase):
    def _types_to_string(self, typ: Type) -> str:
        def _data_type_to_str(dt: DataTypeInfo) -> str:
            if dt.holds:
                return f'{dt.data_type.name}[{self._types_to_string(dt.holds)}]'
            return dt.data_type.name
        return ' | '.join([_data_type_to_str(x) for x in typ.resolved])

    def _generate_function(self, func: Function) -> None:
        mlog.log()
        mlog.log('Function', mlog.bold(func.name))
        with my_nested():
            desc = func.description
            if '\n' in desc:
                desc = desc[:desc.index('\n')]
            mlog.log('Description:', mlog.bold(desc))
            mlog.log('Return type:', mlog.bold(self._types_to_string(func.returns)))
            mlog.log('Pos args:   ', mlog.bold(str([x.name for x in func.posargs])))
            mlog.log('Opt args:   ', mlog.bold(str([x.name for x in func.optargs])))
            mlog.log('Varargs:    ', mlog.bold(func.varargs.name if func.varargs is not None else 'null'))
            mlog.log('Kwargs:     ', mlog.bold(str(list(func.kwargs.keys()))))

    def _generate_object(self, obj: Object) -> None:
        tags = []
        tags += [{
            ObjectType.ELEMENTARY: mlog.yellow('[elementary]'),
            ObjectType.BUILTIN: mlog.green('[builtin]'),
            ObjectType.MODULE: mlog.blue('[module]'),
            ObjectType.RETURNED: mlog.cyan('[returned]'),
        }[obj.obj_type]]
        if obj.is_container:
            tags += [mlog.red('[container]')]
        mlog.log()
        mlog.log('Object', mlog.bold(obj.name), *tags)
        with my_nested():
            desc = obj.description
            if '\n' in desc:
                desc = desc[:desc.index('\n')]
            mlog.log('Description:', mlog.bold(desc))
            mlog.log('Returned by:', mlog.bold(str([x.name for x in obj.returned_by])))
            mlog.log('Methods:')
            with my_nested():
                for m in obj.methods:
                    self._generate_function(m)

    def generate(self) -> None:
        mlog.log('\n\n', mlog.bold('=== Functions ==='), '\n')
        for f in self.functions:
            self._generate_function(f)
        mlog.log('\n\n', mlog.bold('=== Elementary ==='), '\n')
        for obj in self.elementary:
            self._generate_object(obj)
        mlog.log('\n\n', mlog.bold('=== Builtins ==='), '\n')
        for obj in self.builtins:
            self._generate_object(obj)
        mlog.log('\n\n', mlog.bold('=== Returned objects ==='), '\n')
        for obj in self.returned:
            self._generate_object(obj)
        mlog.log('\n\n', mlog.bold('=== Modules ==='), '\n')
        for obj in self.modules:
            self._generate_object(obj)
            for mod_obj in self.extract_returned_by_module(obj):
                self._generate_object(mod_obj)
