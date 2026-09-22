from pprint import pformat

from pygments import highlight
from pygments.formatters import Terminal256Formatter
from pygments.lexers import PythonLexer

import frida

device = frida.get_usb_device()


def trim_icon(icon):
    result = dict(icon)
    result["image"] = result["image"][0:16] + b"..."
    return result


app = device.get_frontmost_application(scope="full")
if app is not None:
    params = dict(app.parameters)
    if "icons" in params:
        params["icons"] = [trim_icon(icon) for icon in params["icons"]]
    print(f"{app.identifier}:", highlight(pformat(params), PythonLexer(), Terminal256Formatter()))
else:
    print("No frontmost application")
