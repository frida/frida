#!/usr/bin/env python3

import shutil, sys

if __name__ == '__main__':
    shutil.copyfile(sys.argv[1], sys.argv[2])
