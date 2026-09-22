#
# Compile example.dylib like this:
# $ clang -shared example.c -o ~/.Trash/example.dylib
#
# Then run:
# $ python inject_file.py Twitter ~/.Trash/example.dylib
#

import sys

import frida


def on_uninjected(id):
    print("on_uninjected id=%u" % id)


target, library_path = sys.argv[1:]

device = frida.get_local_device()
device.on("uninjected", on_uninjected)
id = device.inject_library_file(target, library_path, "example_main", "w00t")
print("*** Injected, id=%u -- hit Ctrl+D to exit!" % id)
sys.stdin.read()
