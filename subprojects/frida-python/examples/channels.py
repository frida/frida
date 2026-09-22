import frida

device = frida.get_usb_device()

channel = device.open_channel("tcp:21")
print("Got channel:", channel)

welcome = channel.read(512)
print("Got welcome message:", welcome)

channel.write_all(b"CWD foo")
reply = channel.read(512)
print("Got reply:", reply)

channel.close()
print("Channel now:", channel)
