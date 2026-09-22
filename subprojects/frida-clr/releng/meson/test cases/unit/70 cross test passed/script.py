#!/usr/bin/env python3

import subprocess
import sys

if __name__ == "__main__":
    sys.exit(subprocess.run(sys.argv[1:]).returncode)
