import { hexdump } from "./hexdump.js";

export class Console {
    #counters = new Map<string, number>();

    info(...args: any[]) {
        sendLogMessage("info", args);
    }

    log(...args: any[]) {
        sendLogMessage("info", args);
    }

    debug(...args: any[]) {
        sendLogMessage("debug", args);
    }

    warn(...args: any[]) {
        sendLogMessage("warning", args);
    }

    error(...args: any[]) {
        sendLogMessage("error", args);
    }

    count(label = "default") {
        const newValue = (this.#counters.get(label) ?? 0) + 1;
        this.#counters.set(label, newValue);
        this.log(`${label}: ${newValue}`);
    }

    countReset(label = "default") {
        if (this.#counters.has(label)) {
            this.#counters.delete(label);
        } else {
            this.warn(`Count for "${label}" does not exist`);
        }
    }
}

type LogLevel = "info" | "debug" | "warning" | "error";

function sendLogMessage(level: LogLevel, values: any[]) {
    const text = values.map(parseLogArgument).join(" ");
    const message = {
        type: "log",
        level: level,
        payload: text
    };
    _send(JSON.stringify(message), null);
}

function parseLogArgument(value: any) {
    if (value instanceof ArrayBuffer)
        return hexdump(value);

    if (value === undefined)
        return "undefined";

    if (value === null)
        return "null";

    return value;
}

declare function _send(json: string, data: ArrayBuffer | null): void;
