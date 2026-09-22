import sys

import frida


def on_message(message):
    print("on_message:", message)


device = frida.get_usb_device()

opengl = device.open_service("dtx:com.apple.instruments.server.services.graphics.opengl")
opengl.on("message", on_message)
opengl.request(
    {
        "method": "setSamplingRate:",
        "args": [5.0],
    }
)
opengl.request(
    {
        "method": "startSamplingAtTimeInterval:",
        "args": [0.0],
    }
)

sys.stdin.read()
