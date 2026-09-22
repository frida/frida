import sys

import frida


def on_detached():
    print("on_detached")


def on_detached_with_reason(reason):
    print("on_detached_with_reason:", reason)


def on_detached_with_varargs(*args):
    print("on_detached_with_varargs:", args)


session = frida.attach("Twitter")
print("attached")
session.on("detached", on_detached)
session.on("detached", on_detached_with_reason)
session.on("detached", on_detached_with_varargs)
sys.stdin.read()
