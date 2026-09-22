import pprint
import sys
from threading import Thread

import frida


def main():
    device = frida.get_usb_device()

    stdout_uuid, stdout_stream = create_stdio_socket(device)
    stderr_uuid, stderr_stream = create_stdio_socket(device)

    appservice = device.open_service("xpc:com.apple.coredevice.appservice")
    response = appservice.request(
        {
            "CoreDevice.featureIdentifier": "com.apple.coredevice.feature.launchapplication",
            "CoreDevice.action": {},
            "CoreDevice.input": {
                "applicationSpecifier": {
                    "bundleIdentifier": {"_0": "no.oleavr.HelloIOS"},
                },
                "options": {
                    "arguments": [],
                    "environmentVariables": {},
                    "standardIOUsesPseudoterminals": True,
                    "startStopped": False,
                    "terminateExisting": True,
                    "user": {"active": True},
                    "platformSpecificOptions": b'<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict/></plist>',
                },
                "standardIOIdentifiers": {
                    "standardOutput": ("uuid", stdout_uuid),
                    "standardError": ("uuid", stderr_uuid),
                },
            },
        }
    )
    pprint.pp(response)

    workers = set()
    for stream, sink in {(stdout_stream, sys.stdout), (stderr_stream, sys.stderr)}:
        t = Thread(target=process_console_output, args=(stream, sink))
        t.start()
        workers.add(t)
    for worker in workers:
        worker.join()


def create_stdio_socket(device):
    stream = device.open_channel("tcp:com.apple.coredevice.openstdiosocket")
    return (stream.read_all(16), stream)


def process_console_output(stream, sink):
    while True:
        chunk = stream.read(4096)
        if not chunk:
            break
        sink.write(chunk.decode("utf-8"))


if __name__ == "__main__":
    main()
