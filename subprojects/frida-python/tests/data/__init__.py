import os
import platform
import sys

system = platform.system()
if system == "Windows":
    target_program = r"C:\Windows\notepad.exe"
elif system == "Darwin":
    target_program = os.path.join(os.path.dirname(__file__), "unixvictim-macos")
elif system == "Linux" and platform.machine() == "x86_64":
    arch = "x86_64" if sys.maxsize > 2**32 else "x86"
    target_program = os.path.join(os.path.dirname(__file__), "unixvictim-" + system.lower() + "-" + arch)
else:
    target_program = "/bin/cat"


__all__ = ["target_program"]
