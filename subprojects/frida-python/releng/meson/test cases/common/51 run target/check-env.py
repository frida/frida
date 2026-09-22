#!/usr/bin/env python3

import os, sys
from pathlib import Path

assert 'MESON_SOURCE_ROOT' in os.environ
assert 'MESON_BUILD_ROOT' in os.environ
assert 'MESON_SUBDIR' in os.environ
assert 'MESONINTROSPECT' in os.environ
assert 'MY_ENV' in os.environ

# Environment has absolute paths and argv has relative paths when using ninja
# backend and absolute paths when using vs backend. What matters is once
# resolved they point to same location.
env_source_root = Path(os.environ['MESON_SOURCE_ROOT']).resolve()
env_build_root = Path(os.environ['MESON_BUILD_ROOT']).resolve()
env_current_source_dir = Path(env_source_root, os.environ['MESON_SUBDIR']).resolve()

print(sys.argv)
argv_paths = [Path(i).resolve() for i in sys.argv[1:]]
source_root, build_root, current_source_dir = argv_paths

print(f'{source_root} == {env_source_root}')
assert source_root == env_source_root
print(f'{build_root} == {env_build_root}')
assert build_root == env_build_root
print(f'{current_source_dir} == {env_current_source_dir}')
assert current_source_dir == env_current_source_dir
