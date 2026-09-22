import frida

device = frida.get_usb_device()
device.unpair()
