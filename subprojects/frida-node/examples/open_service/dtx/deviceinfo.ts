import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();

    const deviceinfo = await device.openService("dtx:com.apple.instruments.server.services.deviceinfo");
    const response = await deviceinfo.request({ method: "runningProcesses" });
    console.log(inspect(response, {
        colors: true,
        depth: Infinity,
        maxArrayLength: Infinity
    }));
}

main()
    .catch(e => {
        console.error(e);
    });
