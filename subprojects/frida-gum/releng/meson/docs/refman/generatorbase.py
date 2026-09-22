# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team


from abc import ABCMeta, abstractmethod
import typing as T

from .model import ReferenceManual, Function, Method, Object, ObjectType, NamedObject

_N = T.TypeVar('_N', bound=NamedObject)

class GeneratorBase(metaclass=ABCMeta):
    def __init__(self, manual: ReferenceManual) -> None:
        self.manual = manual

    @abstractmethod
    def generate(self) -> None:
        pass

    @staticmethod
    def brief(raw: _N) -> str:
        desc_lines = raw.description.split('\n')
        brief = desc_lines[0]
        if '.' in brief and '[[' not in brief:
            brief = brief[:brief.index('.')]
        return brief.strip()

    @staticmethod
    def sorted_and_filtered(raw: T.List[_N]) -> T.List[_N]:
        def key_fn(fn: NamedObject) -> str:
            if isinstance(fn, Method):
                return f'1_{fn.obj.name}.{fn.name}'
            return f'0_{fn.name}'
        return sorted([x for x in raw if not x.hidden], key=key_fn)

    @staticmethod
    def _extract_meson_version() -> str:
        from mesonbuild.coredata import version
        return version

    @property
    def functions(self) -> T.List[Function]:
        return GeneratorBase.sorted_and_filtered(self.manual.functions)

    @property
    def objects(self) -> T.List[Object]:
        return GeneratorBase.sorted_and_filtered(self.manual.objects)

    @property
    def elementary(self) -> T.List[Object]:
        return [x for x in self.objects if x.obj_type == ObjectType.ELEMENTARY]

    @property
    def builtins(self) -> T.List[Object]:
        return [x for x in self.objects if x.obj_type == ObjectType.BUILTIN]

    @property
    def returned(self) -> T.List[Object]:
        return [x for x in self.objects if x.obj_type == ObjectType.RETURNED and x.defined_by_module is None]

    @property
    def modules(self) -> T.List[Object]:
        return [x for x in self.objects if x.obj_type == ObjectType.MODULE]

    def extract_returned_by_module(self, module: Object) -> T.List[Object]:
        return [x for x in self.objects if x.obj_type == ObjectType.RETURNED and x.defined_by_module is module]
