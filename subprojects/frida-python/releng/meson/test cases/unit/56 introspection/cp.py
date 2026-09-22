#! /usr/bin/env python3

import sys
from shutil import copyfile
copyfile(*sys.argv[1:])
