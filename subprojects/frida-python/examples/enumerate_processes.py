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


processes = device.enumerate_processes(scope="full")
for proc in processes:
    params = dict(proc.parameters)
    if "icons" in params:
        params["icons"] = [trim_icon(icon) for icon in params["icons"]]
    print(
        f'Process(pid={proc.pid}, name="{proc.name}", parameters={highlight(pformat(params), PythonLexer(), Terminal256Formatter()).rstrip()})'
    )
