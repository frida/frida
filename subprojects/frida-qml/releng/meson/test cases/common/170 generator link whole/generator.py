#!/usr/bin/env python3

import os
import os.path
import sys


def main():
    name = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    out = sys.argv[2]
    hname = os.path.join(out, name + '.h')
    cname = os.path.join(out, name + '.c')
    print(os.getcwd(), hname)
    with open(hname, 'w') as hfile:
        hfile.write('''
#pragma once
#include "export.h"
int DLL_PUBLIC {name}(void);
'''.format(name=name))
    with open(cname, 'w') as cfile:
        cfile.write('''
#include "{name}.h"
int {name}(void) {{
    return {size};
}}
'''.format(name=name, size=len(name)))


if __name__ == '__main__':
    main()
