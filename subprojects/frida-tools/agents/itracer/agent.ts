import {
    TraceBuffer,
    TraceBufferReader,
    TraceSession,
    TraceStrategy,
} from "frida-itrace";

type RawTraceStrategy = RawTraceThreadStrategy | RawTraceRangeStrategy;
type RawTraceThreadStrategy = ["thread", ["id", number] | ["index", number]];
type RawTraceRangeStrategy = ["range", [CodeLocation, CodeLocation | null]]

type CodeLocation =
    | ["address", string]
    | ["module-export", [string, string]]
    | ["module-offset", [string, number]]
    | ["symbol", string]
    ;

const BUFFER_READER_POLL_INTERVAL_MSEC = 10;

class Agent {
    session: TraceSession | null = null;
    buffer: TraceBuffer | null = null;
    reader: TraceBufferReader | null = null;
    drainTimer: NodeJS.Timeout | null = null;

    createBuffer(): string {
        this.buffer = TraceBuffer.create();
        return this.buffer.location;
    }

    openBuffer(location: string) {
        this.buffer = TraceBuffer.open(location);
    }

    launchBufferReader() {
        this.reader = new TraceBufferReader(this.buffer!);
        this.drainTimer = setInterval(this.#drainBuffer, BUFFER_READER_POLL_INTERVAL_MSEC);
    }

    stopBufferReader() {
        clearInterval(this.drainTimer!);
        this.drainTimer = null;

        this.#drainBuffer();

        this.reader = null;
    }

    #drainBuffer = () => {
        const chunk = this.reader!.read();
        if (chunk.byteLength === 0) {
            return;
        }
        send({ type: "itrace:chunk" }, chunk);

        const lost = this.reader!.lost;
        if (lost !== 0) {
            send({ type: "itrace:lost", payload: { lost } });
        }
    };

    launchTraceSession(rawStrategy: RawTraceStrategy) {
        const strategy = parseTraceStrategy(rawStrategy);
        const session = new TraceSession(strategy, this.buffer!);
        this.session = session;

        session.events.on("start", (regSpecs, regValues) => {
            send({ type: "itrace:start", payload: regSpecs }, regValues);
        });
        session.events.on("end", () => {
            send({ type: "itrace:end" });
        });
        session.events.on("compile", block => {
            send({ type: "itrace:compile", payload: block });
        });
        session.events.on("panic", message => {
            console.error(message);
        });

        session.open();
    }

    queryProgramName() {
        return Process.enumerateModules()[0].name;
    }

    listThreads() {
        return Process.enumerateThreads();
    }
}

function parseTraceStrategy(rawStrategy: RawTraceStrategy): TraceStrategy {
    const [kind, params] = rawStrategy;
    switch (kind) {
        case "thread": {
            let thread: ThreadDetails;
            const threads = Process.enumerateThreads();
            switch (params[0]) {
                case "id": {
                    const desiredId = params[1];
                    const th = threads.find(t => t.id === desiredId);
                    if (th === undefined) {
                        throw new Error("invalid thread ID");
                    }
                    thread = th;
                    break;
                }
                case "index": {
                    thread = threads[params[1]];
                    if (thread === undefined) {
                        throw new Error("invalid thread index");
                    }
                    break;
                }
            }
            return {
                type: "thread",
                threadId: thread.id
            };
        }
        case "range": {
            return {
                type: "range",
                start: parseCodeLocation(params[0]),
                end: parseCodeLocation(params[1])
            };
        }
    }
}

function parseCodeLocation(location: CodeLocation | null): NativePointer {
    if (location === null) {
        return NULL;
    }

    const [kind, params] = location;
    switch (kind) {
        case "address": {
            const address = ptr(params);
            try {
                address.readVolatile(1);
            } catch (e) {
                throw new Error(`invalid address: ${address}`);
            }
            return address;
        }
        case "module-export":
            return Process.getModuleByName(params[0]).getExportByName(params[1]);
        case "module-offset":
            return Process.getModuleByName(params[0]).base.add(params[1]);
        case "symbol": {
            const name = params;
            const { address } = DebugSymbol.fromName(name);
            if (!address.isNull()) {
                return address;
            }
            return Module.getGlobalExportByName(name);
        }
    }
}

const agent = new Agent();

const agentMethodNames = Object.getOwnPropertyNames(Object.getPrototypeOf(agent))
    .filter(name => name !== "constructor");
for (const name of agentMethodNames) {
    rpc.exports[name] = (agent as any)[name].bind(agent);
}
