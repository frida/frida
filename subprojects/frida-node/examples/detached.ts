import frida from "frida";
import readline from "readline";

let input: readline.Interface | null = null;

async function main() {
    const device = await frida.getUsbDevice();
    const session = await device.attach("Hello");
    session.detached.connect(onDetached);

    console.log("[*] Attached. Hit ENTER to exit.");

    input = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        terminal: true
    });
    input.on("line", () => {
        session.detach();
        input!.close();
    });
}

function onDetached(reason: frida.SessionDetachReason, crash: frida.Crash | null) {
    console.log("[*] onDetached() reason:", reason, "crash:", crash);
    input!.close();
}

main()
    .catch(e => {
        console.error(e);
    });
