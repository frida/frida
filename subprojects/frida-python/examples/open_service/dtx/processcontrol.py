import sys

import frida


def on_message(message):
    print("on_message:", message)


device = frida.get_usb_device()

control = device.open_service("dtx:com.apple.instruments.server.services.processcontrol")
control.on("message", on_message)
pid = control.request(
    {
        "method": "launchSuspendedProcessWithDevicePath:bundleIdentifier:environment:arguments:options:",
        "args": [
            "",
            "no.oleavr.HelloIOS",
            {},
            [],
            {
                "StartSuspendedKey": False,
            },
        ],
    }
)
control.request({"method": "startObservingPid:", "args": [pid]})

print(f"App spawned, PID: {pid}.  Kill it to see an example message being emitted.")
sys.stdin.read()
