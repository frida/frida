#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2018 The Meson development team

'''
Regenerate markdown docs by using `meson.py` from the root dir
'''

import argparse
import os
import re
import subprocess
import sys
import textwrap
import json
import typing as T
from pathlib import Path
from urllib.request import urlopen

PathLike = T.Union[Path,str]

def _get_meson_output(root_dir: Path, args: T.List) -> str:
    env = os.environ.copy()
    env['COLUMNS'] = '80'
    return subprocess.run([str(sys.executable), str(root_dir/'meson.py')] + args, check=True, capture_output=True, text=True, env=env).stdout.strip()

def get_commands(help_output: str) -> T.Set[str]:
    # Python's argument parser might put the command list to its own line. Or it might not.
    assert(help_output.startswith('usage: '))
    lines = help_output.split('\n')
    line1 = lines[0]
    line2 = lines[1]
    if '{' in line1:
        cmndline = line1
    else:
        assert('{' in line2)
        cmndline = line2
    cmndstr = cmndline.split('{')[1]
    assert('}' in cmndstr)
    help_commands = set(cmndstr.split('}')[0].split(','))
    assert(len(help_commands) > 0)
    return {c.strip() for c in help_commands}

def get_commands_data(root_dir: Path) -> T.Dict[str, T.Any]:
    usage_start_pattern = re.compile(r'^usage: ', re.MULTILINE)
    positional_start_pattern = re.compile(r'^positional arguments:[\t ]*[\r\n]+', re.MULTILINE)
    options_start_pattern = re.compile(r'^(optional arguments|options):[\t ]*[\r\n]+', re.MULTILINE)
    commands_start_pattern = re.compile(r'^[A-Za-z ]*[Cc]ommands:[\t ]*[\r\n]+', re.MULTILINE)

    def get_next_start(iterators: T.Sequence[T.Any], end: T.Optional[int]) -> int:
        return next((i.start() for i in iterators if i), end)

    def normalize_text(text: str) -> str:
        # clean up formatting
        out = text
        out = re.sub(r'\r\n', r'\r', out, flags=re.MULTILINE) # replace newlines with a linux EOL
        out = re.sub(r'^ +$', '', out, flags=re.MULTILINE) # remove trailing whitespace
        out = re.sub(r'(?:^\n+|\n+$)', '', out) # remove trailing empty lines
        return out

    def parse_cmd(cmd: str) -> T.Dict[str, str]:
        cmd_len = len(cmd)
        usage = usage_start_pattern.search(cmd)
        positionals = positional_start_pattern.search(cmd)
        options = options_start_pattern.search(cmd)
        commands = commands_start_pattern.search(cmd)

        arguments_start = get_next_start([positionals, options, commands], None)
        assert arguments_start

        # replace `usage:` with `$` and dedent
        dedent_size = (usage.end() - usage.start()) - len('$ ')
        usage_text = textwrap.dedent(f'{dedent_size * " "}$ {normalize_text(cmd[usage.end():arguments_start])}')

        return {
            'usage': usage_text,
            'arguments': normalize_text(cmd[arguments_start:cmd_len]),
        }

    def clean_dir_arguments(text: str) -> str:
        # Remove platform specific defaults
        args = [
            'prefix',
            'bindir',
            'datadir',
            'includedir',
            'infodir',
            'libdir',
            'libexecdir',
            'localedir',
            'localstatedir',
            'mandir',
            'sbindir',
            'sharedstatedir',
            'sysconfdir'
        ]
        out = text
        for a in args:
            out = re.sub(r'(--' + a + r' .+?)\s+\(default:.+?\)(\.)?', r'\1\2', out, flags=re.MULTILINE|re.DOTALL)
        return out

    output = _get_meson_output(root_dir, ['--help'])
    commands = get_commands(output)
    commands.remove('help')

    cmd_data = dict()

    for cmd in commands:
        cmd_output = _get_meson_output(root_dir, [cmd, '--help'])
        cmd_data[cmd] = parse_cmd(cmd_output)
        if cmd in ['setup', 'configure']:
            cmd_data[cmd]['arguments'] = clean_dir_arguments(cmd_data[cmd]['arguments'])

    return cmd_data

def generate_hotdoc_includes(root_dir: Path, output_dir: Path) -> None:
    cmd_data = get_commands_data(root_dir)

    for cmd, parsed in cmd_data.items():
        for typ in parsed.keys():
            with open(output_dir / (cmd+'_'+typ+'.inc'), 'w', encoding='utf-8') as f:
                f.write(parsed[typ])

def generate_wrapdb_table(output_dir: Path) -> None:
    url = urlopen('https://wrapdb.mesonbuild.com/v2/releases.json')
    releases = json.loads(url.read().decode())
    with open(output_dir / 'wrapdb-table.md', 'w', encoding='utf-8') as f:
        f.write('| Project | Versions | Provided dependencies | Provided programs |\n')
        f.write('| ------- | -------- | --------------------- | ----------------- |\n')
        for name, info in releases.items():
            versions = []
            added_tags = set()
            for v in info['versions']:
                tag, build = v.rsplit('-', 1)
                if tag not in added_tags:
                    added_tags.add(tag)
                    versions.append(f'[{v}](https://wrapdb.mesonbuild.com/v2/{name}_{v}/{name}.wrap)')
            # Highlight latest version.
            versions_str = f'<big>**{versions[0]}**</big><br/>' + ', '.join(versions[1:])
            dependency_names = info.get('dependency_names', [])
            dependency_names_str = ', '.join(dependency_names)
            program_names = info.get('program_names', [])
            program_names_str = ', '.join(program_names)
            f.write(f'| {name} | {versions_str} | {dependency_names_str} | {program_names_str} |\n')

def regenerate_docs(output_dir: PathLike,
                    dummy_output_file: T.Optional[PathLike]) -> None:
    if not output_dir:
        raise ValueError(f'Output directory value is not set')

    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    root_dir = Path(__file__).resolve().parent.parent

    generate_hotdoc_includes(root_dir, output_dir)
    generate_wrapdb_table(output_dir)

    if dummy_output_file:
        with open(output_dir/dummy_output_file, 'w', encoding='utf-8') as f:
            f.write('dummy file for custom_target output')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate meson docs')
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--dummy-output-file', type=str)

    args = parser.parse_args()

    regenerate_docs(output_dir=args.output_dir,
                    dummy_output_file=args.dummy_output_file)
