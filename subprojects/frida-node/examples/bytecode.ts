import frida from "frida";

const processName = process.argv[2];

const source = `
rpc.exports = {
  listThreads() {
    return Process.enumerateThreads();
  }
};
`;

async function main() {
    const systemSession = await frida.attach(0);
    const bytecode = await systemSession.compileScript(source, {
        name: "bytecode-example"
    });

    const session = await frida.attach(processName);
    const script = await session.createScriptFromBytes(bytecode);
    script.message.connect(message => {
        console.log("[*] Message:", message);
    });
    await script.load();

    console.log("[*] Called listThreads() =>", await script.exports.listThreads());

    await script.unload();
}

main()
    .catch(e => {
        console.error(e);
    });
