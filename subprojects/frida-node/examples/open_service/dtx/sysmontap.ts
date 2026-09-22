import frida from "frida";

let sysmon: frida.Service | null = null;

async function main() {
    const device = await frida.getUsbDevice();

    sysmon = await device.openService("dtx:com.apple.instruments.server.services.sysmontap");
    sysmon.message.connect(onMessage);
    await sysmon.request({ method: "setConfig:", args: [{ ur: 1000, cpuUsage: true, sampleInterval: 1000000000 }] });
    await sysmon.request({ method: "start" });
    await sleep(5000);
    await sysmon.request({ method: "stop" });
    await sleep(1000);
    await sysmon.cancel();
}

function onMessage(message: any) {
    console.log("onMessage:", message);
}

function sleep(duration: number) {
    return new Promise(resolve => {
        setTimeout(resolve, duration);
    });
}

main()
    .catch(e => {
        console.error(e);
    });
