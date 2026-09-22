import frida from "frida";

async function main() {
    const cancellable = new frida.Cancellable();

    setTimeout(() => {
        console.log("Cancelling");
        cancellable.cancel();
    }, 2000);

    const device = await frida.getDevice("xyz", { timeout: 10000 }, cancellable);
    console.log("[*] Device:", device);
}

main()
    .catch(e => {
        console.error(e);
    });
