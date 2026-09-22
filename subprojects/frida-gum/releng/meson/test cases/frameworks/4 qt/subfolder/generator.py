#!/usr/bin/env python3
import sys

if len(sys.argv) > 1:
    with open(sys.argv[1], "w") as output:
        output.write("Hello World")
