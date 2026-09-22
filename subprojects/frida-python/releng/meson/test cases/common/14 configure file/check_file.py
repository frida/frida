#!/usr/bin/env python3

import os
import sys

def permit_osx_workaround(m1,  m2):
    import platform
    if platform.system().lower() != 'darwin':
        return False
    if m2 % 10000 != 0:
        return False
    if m1//10000 != m2//10000:
        return False
    return True

if len(sys.argv) == 2:
    assert os.path.exists(sys.argv[1])
elif len(sys.argv) == 3:
    f1 = sys.argv[1]
    f2 = sys.argv[2]
    m1 = os.stat(f1).st_mtime_ns
    m2 = os.stat(f2).st_mtime_ns
    # Compare only os.stat()
    if m1 != m2:
        # Under macOS the lower four digits sometimes get assigned
        # zero, even though shutil.copy2 should preserve metadata.
        # Just have to accept it, I guess.
        if not permit_osx_workaround(m1, m2):
            raise RuntimeError(f'mtime of {f1!r} ({m1!r}) != mtime of {f2!r} ({m2!r})')
    import filecmp
    if not filecmp.cmp(f1, f2):
        raise RuntimeError(f'{f1!r} != {f2!r}')
else:
    raise AssertionError
