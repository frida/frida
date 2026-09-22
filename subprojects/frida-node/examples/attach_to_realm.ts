import frida, { TargetProcess } from "frida";

const processName = process.argv[2];

const source = `
send({ pid: Process.id, arch: Process.arch });
`;

async function main() {
    const device = await frida.getUsbDevice();
    await probeRealm(device, processName, frida.Realm.Native);
    await probeRealm(device, processName, frida.Realm.Emulated);
}

async function probeRealm(device: frida.Device, target: TargetProcess, realm: frida.Realm) {
    const session = await device.attach(target, { realm });

    const script = await session.createScript(source);
    script.message.connect(message => {
        console.log(`[Realm: ${realm}] Message:`, message);
        script.unload();
    });
    await script.load();
    console.log(`[Realm: ${realm}] Script loaded`);
}

main()
    .catch(e => {
        console.error(e);
    });
