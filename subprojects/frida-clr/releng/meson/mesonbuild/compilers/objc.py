# SPDX-License-Identifier: Apache-2.0
# Copyright 2012-2017 The Meson development team

from __future__ import annotations

import typing as T

from .. import coredata
from ..mesonlib import OptionKey

from .compilers import Compiler
from .c import _ALL_STDS, _ClangCStds
from .mixins.clike import CLikeCompiler
from .mixins.gnu import GnuCompiler, gnu_common_warning_args, gnu_objc_warning_args
from .mixins.clang import ClangCompiler

if T.TYPE_CHECKING:
    from ..envconfig import MachineInfo
    from ..environment import Environment
    from ..linkers.linkers import DynamicLinker
    from ..mesonlib import MachineChoice


class ObjCCompiler(CLikeCompiler, Compiler):

    language = 'objc'

    def __init__(self, ccache: T.List[str], exelist: T.List[str], version: str, for_machine: MachineChoice,
                 is_cross: bool, info: 'MachineInfo',
                 linker: T.Optional['DynamicLinker'] = None,
                 full_version: T.Optional[str] = None):
        Compiler.__init__(self, ccache, exelist, version, for_machine, info,
                          is_cross=is_cross, full_version=full_version,
                          linker=linker)
        CLikeCompiler.__init__(self)

    @staticmethod
    def get_display_language() -> str:
        return 'Objective-C'

    def sanity_check(self, work_dir: str, environment: 'Environment') -> None:
        code = '#import<stddef.h>\nint main(void) { return 0; }\n'
        return self._sanity_check_impl(work_dir, environment, 'sanitycheckobjc.m', code)


class GnuObjCCompiler(GnuCompiler, ObjCCompiler):
    def __init__(self, ccache: T.List[str], exelist: T.List[str], version: str, for_machine: MachineChoice,
                 is_cross: bool, info: 'MachineInfo',
                 defines: T.Optional[T.Dict[str, str]] = None,
                 linker: T.Optional['DynamicLinker'] = None,
                 full_version: T.Optional[str] = None):
        ObjCCompiler.__init__(self, ccache, exelist, version, for_machine, is_cross,
                              info, linker=linker, full_version=full_version)
        GnuCompiler.__init__(self, defines)
        default_warn_args = ['-Wall', '-Winvalid-pch']
        self.warn_args = {'0': [],
                          '1': default_warn_args,
                          '2': default_warn_args + ['-Wextra'],
                          '3': default_warn_args + ['-Wextra', '-Wpedantic'],
                          'everything': (default_warn_args + ['-Wextra', '-Wpedantic'] +
                                         self.supported_warn_args(gnu_common_warning_args) +
                                         self.supported_warn_args(gnu_objc_warning_args))}


class ClangObjCCompiler(ClangCompiler, ObjCCompiler):
    def __init__(self, ccache: T.List[str], exelist: T.List[str], version: str, for_machine: MachineChoice,
                 is_cross: bool, info: 'MachineInfo',
                 defines: T.Optional[T.Dict[str, str]] = None,
                 linker: T.Optional['DynamicLinker'] = None,
                 full_version: T.Optional[str] = None):
        ObjCCompiler.__init__(self, ccache, exelist, version, for_machine, is_cross,
                              info, linker=linker, full_version=full_version)
        ClangCompiler.__init__(self, defines)
        self._clang_objcstds = _ClangObjCStds(version, for_machine)
        default_warn_args = ['-Wall', '-Winvalid-pch']
        self.warn_args = {'0': [],
                          '1': default_warn_args,
                          '2': default_warn_args + ['-Wextra'],
                          '3': default_warn_args + ['-Wextra', '-Wpedantic'],
                          'everything': ['-Weverything']}

    def get_options(self) -> 'coredata.MutableKeyedOptionDictType':
        opts = super().get_options()
        opts.update(self._clang_objcstds.get_options())
        return opts

    def get_option_compile_args(self, options: 'coredata.KeyedOptionDictType') -> T.List[str]:
        args = []
        std = options[OptionKey('std', machine=self.for_machine, lang='c')]
        if std.value != 'none':
            args.append('-std=' + std.value)
        return args

class _ClangObjCStdsBase:
    language = 'c'

    def __init__(self, version: str, for_machine: MachineChoice):
        self.version = version
        self.for_machine = for_machine

    def get_options(self) -> 'coredata.MutableKeyedOptionDictType':
        key = OptionKey('std', machine=self.for_machine, lang=self.language)
        return {key: coredata.UserStdOption('C', _ALL_STDS)}

class _ClangObjCStds(_ClangCStds, _ClangObjCStdsBase):
    def __init__(self, version: str, for_machine: MachineChoice):
        _ClangObjCStdsBase.__init__(self, version, for_machine)

    def get_output_args(self, outputname: str) -> T.List[str]:
        pass

    def get_optimization_args(self, optimization_level: str) -> T.List[str]:
        return []

    def sanity_check(self, work_dir: str, environment: 'Environment') -> None:
        pass

class AppleClangObjCCompiler(ClangObjCCompiler):

    """Handle the differences between Apple's clang and vanilla clang."""
