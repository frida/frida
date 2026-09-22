import frida from "frida";

let device: frida.Device | null = null;

async function main() {
    device = await frida.getLocalDevice();
    device.childAdded.connect(onChildAdded);
    device.childRemoved.connect(onChildRemoved);
    device.output.connect(onOutput);

    await showPendingChildren();

    console.log("[*] spawn()");
    const pid = await device.spawn("/bin/sh", {
        argv: ["/bin/sh", "-c", "ls /"],
        env: {
            "BADGER": "badger-badger-badger",
            "SNAKE": "true",
            "AGE": "42",
        },
        cwd: "/usr",
        stdio: frida.Stdio.Pipe,
        aslr: "auto"
    });
    console.log(`[*] attach(${pid})`);
    const session = await device.attach(pid);
    console.log("[*] enableChildGating()");
    await session.enableChildGating();
    console.log(`[*] resume(${pid})`);
    await device.resume(pid);
}

async function onChildAdded(child: frida.Child) {
    try {
        console.log("[*] onChildAdded:", child);

        await showPendingChildren();

        console.log(`[*] resume(${child.pid})`);
        const session = await device!.attach(child.pid);
        session.detached.connect(onChildDetached);
        await device!.resume(child.pid);
    } catch (e) {
        console.error(e);
    }
}

function onChildRemoved(child: frida.Child) {
    console.log("[*] onChildRemoved:", child);
}

function onOutput(pid: frida.ProcessID, fd: frida.FileDescriptor, data: Buffer) {
    let description: string;
    if (data.length > 0) {
        description = "\"" + data.toString().replace(/\n/g, "\\n") + "\"";
    } else {
        description = "<EOF>";
    }
    console.log(`[*] onOutput(pid=${pid}, fd=${fd}, data=${description})`);
}

function onChildDetached(reason: frida.SessionDetachReason, crash: frida.Crash | null) {
    console.log(`[*] onChildDetached(reason="${reason}")`);

    device!.childAdded.disconnect(onChildAdded);
    device!.childRemoved.disconnect(onChildRemoved);
    device!.output.disconnect(onOutput);
}

async function showPendingChildren() {
    const pending = await device!.enumeratePendingChildren();
    console.log("[*] enumeratePendingChildren():", pending);
}

main()
    .catch(e => {
        console.error(e);
    });
