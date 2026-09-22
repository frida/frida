import os
import platform

system = platform.system()
if system == "Windows":
    target_program = r"C:\Windows\notepad.exe"
elif system == "Darwin":
    target_program = os.path.join(os.path.dirname(__file__), "unixvictim-macos")
else:
    target_program = "/bin/cat"

__all__ = ["target_program"]
