import frida from "frida";

async function main() {
    const deviceManager = frida.getDeviceManager();

    const device = await deviceManager.addRemoteDevice("127.0.0.1:1337", {
        certificate: "/home/oleavr/src/cert.pem",
        token: "hunter2",
        keepaliveInterval: 1337
    });
    console.log("[*] Added:", device);

    const processes = await device.enumerateProcesses();
    console.log("[*] Processes:", processes);

    await deviceManager.removeRemoteDevice("127.0.0.1:1337");
}

main()
    .catch(e => {
        console.error(e);
    });
