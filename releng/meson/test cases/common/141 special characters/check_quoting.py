#!/usr/bin/env python3

import sys

expected = {
    'newline': '\n',
    'dollar': '$',
    'colon': ':',
    'space': ' ',
    'multi1': '  ::$$  ::$$',
    'multi2': '  ::$$\n\n  \n\n::$$',
}

output = None

for arg in sys.argv[1:]:
    try:
        name, value = arg.split('=', 1)
    except ValueError:
        output = arg
        continue

    if expected[name] != value:
        raise RuntimeError('{!r} is {!r} but should be {!r}'.format(name, value, expected[name]))

if output is not None:
    with open(output, 'w') as f:
        f.write('Success!')
