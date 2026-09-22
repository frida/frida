import frida from "frida";

let deviceManager: frida.DeviceManager | null = null;

async function main() {
    deviceManager = frida.getDeviceManager();

    deviceManager.added.connect(onAdded);
    deviceManager.removed.connect(onRemoved);
    deviceManager.changed.connect(onChanged);

    process.on("SIGTERM", stop);
    process.on("SIGINT", stop);

    const devices = await deviceManager.enumerateDevices();
    console.log("[*] Called enumerateDevices() =>", devices);
}

function stop() {
    deviceManager!.added.disconnect(onAdded);
    deviceManager!.removed.disconnect(onRemoved);
    deviceManager!.changed.disconnect(onChanged);
}

function onAdded(device: frida.Device) {
    console.log("[*] Added:", device);
}

function onRemoved(device: frida.Device) {
    console.log("[*] Removed:", device);
}

function onChanged() {
    console.log("[*] Changed");
}

main()
    .catch(e => {
        console.error(e);
    });
