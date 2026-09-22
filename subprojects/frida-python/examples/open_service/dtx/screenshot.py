import sys

import frida

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} outfile.png", file=sys.stderr)
    sys.exit(1)
outfile = sys.argv[1]

device = frida.get_usb_device()

screenshot = device.open_service("dtx:com.apple.instruments.server.services.screenshot")
png = screenshot.request({"method": "takeScreenshot"})
with open(outfile, "wb") as f:
    f.write(png)
