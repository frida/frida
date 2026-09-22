import frida from "frida";

async function main() {
    const device = await frida.getUsbDevice();
    device.processCrashed.connect(onProcessCrashed);

    const session = await device.attach("Hello");
    session.detached.connect(onSessionDetached);

    console.log("[*] Ready");
}

function onProcessCrashed(crash: frida.Crash) {
    console.log("[*] onProcessCrashed() crash:", crash);
    console.log(crash.report);
}

function onSessionDetached(reason: frida.SessionDetachReason, crash: frida.Crash | null) {
    console.log("[*] onDetached() reason:", reason, "crash:", crash);
}

main()
    .catch(e => {
        console.error(e);
    });
