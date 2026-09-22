#!/usr/bin/env python3

import os
import sys

assert os.environ['TEST_VAR'] == sys.argv[1]
