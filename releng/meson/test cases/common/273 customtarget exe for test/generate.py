#!/usr/bin/env python3

import os
import sys

program = '''#!/usr/bin/env python3

raise SystemExit({})
'''

for i, a in enumerate(sys.argv[1:]):
    with open(a, 'w') as f:
        print(program.format(i), file=f)
    os.chmod(a, 0o755)
