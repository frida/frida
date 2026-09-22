import frida from "frida";
import { targetProgram } from "./data/index.js";

export class LabRat {
    private constructor(private _pid: number) {
    }

    get pid(): number {
        return this._pid;
    }

    static async start(): Promise<LabRat> {
        const pid = await frida.spawn(targetProgram(), {
            stdio: frida.Stdio.Pipe
        });
        await frida.resume(pid);
        // TODO: improve injectors to handle injection into a process that hasn't yet finished initializing
        await sleep(50);

        return new LabRat(pid);
    }

    stop(): void {
        frida.kill(this._pid);
    }
}

function sleep(delay: number): Promise<void> {
    return new Promise(resolve => {
        setTimeout(() => { resolve(); }, delay);
    });
}
