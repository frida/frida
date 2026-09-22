import frida from "frida";

let control: frida.Service | null = null;

async function main() {
    const device = await frida.getUsbDevice();

    control = await device.openService("dtx:com.apple.instruments.server.services.processcontrol");
    control.message.connect(onMessage);
    const pid = await control.request({
        method: "launchSuspendedProcessWithDevicePath:bundleIdentifier:environment:arguments:options:",
        args: [
            "",
            "no.oleavr.HelloIOS",
            {},
            [],
            {
                StartSuspendedKey: false,
            }
        ]
    });
    await control.request({ method: "startObservingPid:", args: [pid] });

    console.log(`App spawned, PID: ${pid}.  Kill it to see an example message being emitted.`);
}

function onMessage(message: any) {
    console.log("onMessage:", message);
    control!.cancel();
}

main()
    .catch(e => {
        console.error(e);
    });
