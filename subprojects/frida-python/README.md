# frida-python

Python bindings for [Frida](https://frida.re).

# Some tips during development

To build and test your own wheel, do something along the following lines:

```
set FRIDA_VERSION=16.0.1-dev.7 # from C:\src\frida\build\tmp-windows\frida-version.h
set FRIDA_EXTENSION=C:\src\frida\build\frida-windows\x64-Release\lib\python3.10\site-packages\_frida.pyd
cd C:\src\frida\frida-python\
pip wheel .
pip uninstall frida
pip install frida-16.0.1.dev7-cp34-abi3-win_amd64.whl
```
