# SPDX-License-Identifier: Apache-2.0

import argparse
import textwrap

parser = argparse.ArgumentParser()
parser.add_argument('output')
args = parser.parse_args()

with open(args.output, 'w') as f:
    f.write(textwrap.dedent('''\
        cpdef func():
            return "Hello, World!"
        '''))
