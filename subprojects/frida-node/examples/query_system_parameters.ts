import frida from "frida";

async function main() {
    console.log("Local parameters:", await frida.querySystemParameters());

    const device = await frida.getUsbDevice();
    console.log("USB device parameters:", await device.querySystemParameters());
}

main()
    .catch(e => {
        console.error(e);
    });
