import subprocess
import sys


def main():
    process = subprocess.run(sys.argv[1:])
    sys.exit(process.returncode)


if __name__ == "__main__":
    main()
