import os
from pathlib import Path
import platform
import subprocess
import sys


def main():
    runner_env = { **os.environ }

    if platform.system() == 'Windows':
        runner_program = Path(sys.argv[1])
        gumpp_dir = runner_program.parent.parent / "bindings" / "gumpp"
        if gumpp_dir.exists():
            runner_env["PATH"] = str(gumpp_dir) + os.pathsep + runner_env["PATH"]

    process = subprocess.run(sys.argv[1:], env=runner_env)

    sys.exit(process.returncode)


if __name__ == "__main__":
    main()
