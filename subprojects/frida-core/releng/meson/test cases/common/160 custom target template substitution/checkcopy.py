#!/usr/bin/env python3

import sys
import shutil

if '@INPUT1@' in sys.argv[1]:
    shutil.copyfile(sys.argv[2], sys.argv[3])
else:
    sys.exit('String @INPUT1@ not found in "{}"'.format(sys.argv[1]))
