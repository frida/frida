import frida from "frida";
import fs from "fs/promises";

let device: frida.Device | null = null;
const tracers: Tracer[] = [];

async function main() {
    device = await frida.getUsbDevice();
    device.spawnAdded.connect(onSpawnAdded);

    console.log("[*] Enabling spawn gating");
    await device.enableSpawnGating();
    console.log("[*] Enabled spawn gating");

    await showPendingSpawn();
}

async function showPendingSpawn() {
    const pending = await device!.enumeratePendingSpawn();
    console.log("[*] enumeratePendingSpawn():", pending);
}

async function onSpawnAdded(spawn: frida.Spawn) {
    try {
        console.log("[*] onSpawnAdded:", spawn);

        await showPendingSpawn();

        if (spawn.identifier === "my.app") {
            console.log("[*] Tracing", spawn.pid);
            const tracer = await Tracer.open(spawn.pid);
            tracers.push(tracer);
        } else {
            console.log("[*] Resuming", spawn.pid);
            await device!.resume(spawn.pid);
        }
    } catch (e) {
        console.error(e);
    }
}

class Tracer {
    #session: frida.Session | null = null;
    #script: frida.Script | null = null;

    static async open(pid: frida.ProcessID) {
        const tracer = new Tracer(pid);
        await tracer._initialize();
        return tracer;
    }

    constructor(public pid: frida.ProcessID) {
    }

    async _initialize() {
        const session = await device!.attach(this.pid);
        this.#session = session;
        session.detached.connect(this._onSessionDetached.bind(this));

        const agentPath = new URL("./spawn_gating_agent.js", import.meta.url).pathname;
        const source = await fs.readFile(agentPath, "utf-8");
        const script = await session.createScript(source);
        this.#script = script;
        script.message.connect(this._onScriptMessage.bind(this));
        await script.load();

        await device!.resume(this.pid);
    }

    _onSessionDetached(reason: frida.SessionDetachReason) {
        console.log(`[PID ${this.pid}] onSessionDetached(reason="${reason}")`);
    }

    _onScriptMessage(message: frida.Message, data: Buffer | null) {
        console.log(`[PID ${this.pid}] onScriptMessage()`, message);
    }
}

main()
    .catch(e => {
        console.error(e);
    });
