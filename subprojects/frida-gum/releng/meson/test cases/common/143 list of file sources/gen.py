import shutil
import sys

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise Exception('Requires exactly 2 args')
    shutil.copy2(sys.argv[1], sys.argv[2])
