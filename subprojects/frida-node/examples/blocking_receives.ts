import frida from "frida";

const [, , processName, processAddress] = process.argv;

const source = `
Interceptor.attach(ptr('@ADDRESS@'), {
  onEnter(args) {
    send(args[0].toInt32());
    const op = recv('input', value => {
      args[0] = ptr(value.payload);
    });
    op.wait();
  }
});
`;

let script: frida.Script | null = null;

async function main() {
    process.on("SIGTERM", stop);
    process.on("SIGINT", stop);

    const session = await frida.attach(processName);

    script = await session.createScript(source.replace("@ADDRESS@", processAddress));
    script.message.connect(message => {
        console.log("[*] Message:", message);
        if (message.type === frida.MessageType.Send) {
            const val = message.payload;
            script!.post({
                type: "input",
                payload: `${(val * 2)}`
            });
        }
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

main()
    .catch(e => {
        console.error(e);
    });
