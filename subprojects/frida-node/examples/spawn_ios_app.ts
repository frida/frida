import frida from "frida";

const current: State = {
    device: null,
    pid: null,
    script: null
};

interface State {
    device: frida.Device | null;
    pid: frida.ProcessID | null;
    script: frida.Script | null;
}

async function main() {
    process.on("SIGTERM", stop);
    process.on("SIGINT", stop);

    const device = await frida.getUsbDevice();
    current.device = device;
    device.output.connect(onOutput);

    console.log("[*] spawn()");
    const pid = await device.spawn("com.atebits.Tweetie2", {
        url: "twitter://user?screen_name=fridadotre",
        env: {
            "OS_ACTIVITY_DT_MODE": "YES",
            "NSUnbufferedIO": "YES"
        },
        stdio: frida.Stdio.Pipe
    });
    current.pid = pid;

    console.log(`[*] attach(${pid})`);
    const session = await device.attach(pid);
    session.detached.connect(onDetached);

    console.log(`[*] createScript()`);
    const script = await session.createScript(`
Interceptor.attach(Module.getExportByName('UIKit', 'UIApplicationMain'), function () {
  send({
    timestamp: Date.now(),
    name: 'UIApplicationMain'
  });
});
`);
    current.script = script;
    script.message.connect(onMessage);
    await script.load();

    console.log(`[*] resume(${pid})`);
    await device.resume(pid);
}

function stop() {
    const { device, script } = current;

    if (script !== null) {
        script.unload();
        current.script = null;
    }

    if (device !== null) {
        device.output.disconnect(onOutput);
        current.device = null;
    }
}

function onOutput(pid: frida.ProcessID, fd: frida.FileDescriptor, data: Buffer) {
    if (pid !== current.pid)
        return;

    let description: string;
    if (data.length > 0) {
        description = "\"" + data.toString().replace(/\n/g, "\\n") + "\"";
    } else {
        description = "<EOF>";
    }
    console.log(`[*] onOutput(pid=${pid}, fd=${fd}, data=${description})`);
}

function onDetached(reason: frida.SessionDetachReason) {
    console.log(`[*] onDetached(reason="${reason}")`);
    current.device!.output.disconnect(onOutput);
}

function onMessage(message: frida.Message, data: Buffer | null) {
    console.log("[*] onMessage() message:", message, "data:", data);
}

main()
    .catch(e => {
        console.error(e);
    });
