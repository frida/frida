import frida from "frida";

async function main() {
    const device = await frida.getUsbDevice();

    const diag = await device.openService("plist:com.apple.mobile.diagnostics_relay");
    await diag.request({ type: "query", payload: { Request: "Sleep", WaitForDisconnect: true } });
    await diag.request({ type: "query", payload: { Request: "Goodbye" } });
}

main()
    .catch(e => {
        console.error(e);
    });
