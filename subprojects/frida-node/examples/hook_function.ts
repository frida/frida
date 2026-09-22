import frida from "frida";

const [, , processName, processAddress] = process.argv;

const source = `
Interceptor.attach(ptr('@ADDRESS@'), {
  onEnter(args) {
    send(args[0].toInt32());
  }
});
`;

let script: frida.Script | null = null;

async function main() {
    process.on("SIGTERM", stop);
    process.on("SIGINT", stop);

    const session = await frida.attach(processName);
    session.detached.connect(onDetached);

    script = await session.createScript(source.replace("@ADDRESS@", processAddress));
    script.message.connect(message => {
        console.log("[*] Message:", message);
    });
    await script.load();
    console.log("[*] Script loaded");
}

function stop() {
    if (script !== null) {
        script.unload();
        script = null;
    }
}

function onDetached(reason: frida.SessionDetachReason) {
    console.log(`[*] onDetached(reason=${reason})`);
}

main()
    .catch(e => {
        console.error(e);
    });
