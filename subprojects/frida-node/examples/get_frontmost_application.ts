import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();
    const application = await device.getFrontmostApplication({ scope: frida.Scope.Full });
    console.log("[*] Frontmost application:", inspect(application, {
        depth: 3,
        colors: true
    }));
}

main()
    .catch(e => {
        console.error(e);
    });
