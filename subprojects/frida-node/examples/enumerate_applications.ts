import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();
    const applications = await device.enumerateApplications({ scope: frida.Scope.Full });
    console.log("[*] Applications:", inspect(applications, {
        maxArrayLength: 500,
        depth: 4,
        colors: true
    }));
}

main()
    .catch(e => {
        console.error(e);
    });
