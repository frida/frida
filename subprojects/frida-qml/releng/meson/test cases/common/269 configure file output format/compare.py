import sys

with open(sys.argv[1], 'r', encoding='utf-8') as f, open(sys.argv[2], 'r', encoding='utf-8') as g:
    if f.read() != g.read():
        sys.exit('contents are not equal')
