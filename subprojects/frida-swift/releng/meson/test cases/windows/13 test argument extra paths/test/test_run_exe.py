import subprocess
import argparse
import sys

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('prog')
    args = parser.parse_args()

    res = subprocess.run(args.prog)

    sys.exit(res.returncode - 42)
