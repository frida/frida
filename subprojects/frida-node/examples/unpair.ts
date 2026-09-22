import frida from "frida";

async function main() {
    const device = await frida.getUsbDevice();
    await device.unpair();
}

main()
    .catch(e => {
        console.error(e);
    });
