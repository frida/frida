#!/usr/bin/env python3

import sys

contents = '''
void print_wrapper(string arg) {
    print (arg);
}
'''

with open(sys.argv[1], 'w') as f:
    f.write(contents)
