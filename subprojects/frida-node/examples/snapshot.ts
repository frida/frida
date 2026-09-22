import frida from "frida";

const embedScript = `
const button = {
  color: 'blue',
};

function mutateButton() {
  button.color = 'red';
}
`;

const warmupScript = `
mutateButton();
`;

const testScript = `
console.log('Button before:', JSON.stringify(button));
mutateButton();
console.log('Button after:', JSON.stringify(button));
`;

const runtime = frida.ScriptRuntime.V8;

async function main() {
    const session = await frida.attach(0);

    const snapshot = await session.snapshotScript(embedScript, { warmupScript, runtime });

    const script = await session.createScript(testScript, { snapshot, runtime });
    script.message.connect(message => {
        console.log("[*] Message:", message);
    });
    await script.load();

    await script.unload();
}

main()
    .catch(e => {
        console.error(e);
    });
