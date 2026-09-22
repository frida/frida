# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 The Meson development team

from pathlib import Path
import argparse
import typing as T

from mesonbuild import mlog

from .loaderbase import LoaderBase
from .loaderpickle import LoaderPickle
from .loaderyaml import LoaderYAML

from .generatorbase import GeneratorBase
from .generatorjson import GeneratorJSON
from .generatorprint import GeneratorPrint
from .generatorpickle import GeneratorPickle
from .generatormd import GeneratorMD
from .generatorman import GeneratorMan
from .generatorvim import GeneratorVim

meson_root = Path(__file__).absolute().parents[2]

def main() -> int:
    parser = argparse.ArgumentParser(description='Meson reference manual generator')
    parser.add_argument('-l', '--loader', type=str, default='yaml', choices=['yaml', 'fastyaml', 'pickle'], help='Information loader backend')
    parser.add_argument('-g', '--generator', type=str, choices=['print', 'pickle', 'md', 'json', 'man', 'vim'], required=True, help='Generator backend')
    parser.add_argument('-s', '--sitemap', type=Path, default=meson_root / 'docs' / 'sitemap.txt', help='Path to the input sitemap.txt')
    parser.add_argument('-o', '--out', type=Path, required=True, help='Output directory for generated files')
    parser.add_argument('-i', '--input', type=Path, default=meson_root / 'docs' / 'yaml', help='Input path for the selected loader')
    parser.add_argument('--link-defs', type=Path, help='Output file for the MD generator link definition file')
    parser.add_argument('--depfile', type=Path, default=None, help='Set to generate a depfile')
    parser.add_argument('-q', '--quiet', action='store_true', help='Suppress verbose output')
    parser.add_argument('--force-color', action='store_true', help='Force enable colors')
    parser.add_argument('--no-modules', action='store_true', help='Disable building modules')
    args = parser.parse_args()

    if args.quiet:
        mlog.set_quiet()

    if args.force_color:
        mlog.colorize_console = lambda: True

    loaders: T.Dict[str, T.Callable[[], LoaderBase]] = {
        'yaml': lambda: LoaderYAML(args.input),
        'fastyaml': lambda: LoaderYAML(args.input, strict=False),
        'pickle': lambda: LoaderPickle(args.input),
    }

    loader = loaders[args.loader]()
    refMan = loader.load()

    generators: T.Dict[str, T.Callable[[], GeneratorBase]] = {
        'print': lambda: GeneratorPrint(refMan),
        'pickle': lambda: GeneratorPickle(refMan, args.out),
        'md': lambda: GeneratorMD(refMan, args.out, args.sitemap, args.link_defs, not args.no_modules),
        'json': lambda: GeneratorJSON(refMan, args.out, not args.no_modules),
        'man': lambda: GeneratorMan(refMan, args.out, not args.no_modules),
        'vim': lambda: GeneratorVim(refMan, args.out),
    }
    generator = generators[args.generator]()

    # Generate the depfile if required
    if args.depfile is not None:
        assert isinstance(args.depfile, Path)
        assert isinstance(args.out, Path)

        # Also add all files of this package
        script_files = list(Path(__file__).resolve().parent.glob('**/*.py'))
        templates = list(Path(__file__).resolve().parent.glob('**/*.mustache'))

        out_text = f'{args.out.resolve().as_posix()}: \\\n'
        for input in loader.input_files + script_files + templates:
            out_text += f'    {input.resolve().as_posix():<93} \\\n'

        args.depfile.write_text(out_text, encoding='utf-8')

    generator.generate()
    return 0
