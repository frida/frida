import frida from "frida";

const processName = process.argv[2];

const source = `
rpc.exports = {
  hello() {
    return 'Hello';
  },
  failPlease() {
    oops;
  }
};
`;

async function main() {
    const session = await frida.attach(processName);

    const script = await session.createScript(source);
    await script.load();

    try {
        const api = script.exports;

        console.log("[*] api.hello() =>", await api.hello());

        await api.failPlease();
    } finally {
        await script.unload();
    }
}

main()
    .catch(e => {
        console.error(e);
    });
