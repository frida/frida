#!/usr/bin/env python3

import os

assert os.environ['MY_PATH'] == os.pathsep.join(['0', '1', '2'])
