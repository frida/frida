import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();

    console.log("Getting channel...");
    // const channel = await device.openChannel("tcp:1234");
    const channel = await device.openChannel("lockdown:com.apple.instruments.remoteserver");
    console.log("Got channel:", inspect(channel, {
        colors: true,
        breakLength: Infinity
    }));
}

main()
    .catch(e => {
        console.error(e.stack);
    });
