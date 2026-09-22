import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();
    const processes = await device.enumerateProcesses({ scope: frida.Scope.Full });
    console.log("[*] Processes:", inspect(processes, {
        maxArrayLength: 500,
        depth: 4,
        colors: true
    }));
}

main()
    .catch(e => {
        console.error(e);
    });
