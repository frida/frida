import frida from "frida";
import readline from "readline";

async function main() {
    const device = await frida.getRemoteDevice();

    const session = await device.attach("hello", {
        persistTimeout: 30
    });
    session.detached.connect(reason => {
        console.log("Detached:", reason);
        showPrompt();
    });

    const script = await session.createScript(`
let _puts = null;

Interceptor.attach(DebugSymbol.getFunctionByName('f'), {
  onEnter(args) {
    const n = args[0].toInt32();
    send(n);
  }
});

rpc.exports.dispose = () => {
  puts('Script unloaded');
};

let serial = 1;
setInterval(() => {
  puts(\`Agent still here! serial=\${serial++}\`);
}, 5000);

function puts(s) {
  if (_puts === null) {
    _puts = new NativeFunction(Module.getExportByName(null, 'puts'), 'int', ['pointer']);
  }
  _puts(Memory.allocUtf8String(s));
}
`);
    script.message.connect(message => {
        console.log("Message:", message);
        showPrompt();
    });
    await script.load();

    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        terminal: true
    });
    rl.on("close", () => {
        session.detach();
    });
    rl.on("line", async command => {
        try {
            if (command === "resume") {
                await session.resume();
                console.log("Resumed!");
            }
        } catch (e) {
            console.error(e);
        }

        showPrompt();
    });
    showPrompt();
}

function showPrompt() {
    process.stdout.write("> ");
}

main()
    .catch(e => {
        console.error(e);
    });
