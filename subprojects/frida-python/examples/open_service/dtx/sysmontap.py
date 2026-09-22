import time

import frida


def on_message(message):
    print("on_message:", message)


device = frida.get_usb_device()

sysmon = device.open_service("dtx:com.apple.instruments.server.services.sysmontap")
sysmon.on("message", on_message)
sysmon.request(
    {
        "method": "setConfig:",
        "args": [
            {
                "ur": 1000,
                "cpuUsage": True,
                "sampleInterval": 1000000000,
            },
        ],
    }
)
sysmon.request({"method": "start"})
time.sleep(5)
sysmon.request({"method": "stop"})
time.sleep(1)
