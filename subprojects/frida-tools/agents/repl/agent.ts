import { encodeValue } from "./value.js";

class REPL {
    #quickCommands = new Map();

    registerQuickCommand(name: string, handler: QuickCommandHandler) {
        this.#quickCommands.set(name, handler);
    }

    unregisterQuickCommand(name: string) {
        this.#quickCommands.delete(name);
    }

    _invokeQuickCommand(tokens: string[]): any {
        const name = tokens[0];
        const handler = this.#quickCommands.get(name);
        if (handler !== undefined) {
            const { minArity, onInvoke } = handler;
            if (tokens.length - 1 < minArity) {
                throw Error(`${name} needs at least ${minArity} arg${(minArity === 1) ? "" : "s"}`);
            }
            return onInvoke(...tokens.slice(1));
        } else {
            throw Error(`Unknown command ${name}`);
        }
    }
}

const repl = new REPL();

globalThis.REPL = repl;
globalThis.cm = null;
globalThis.cs = {};

registerLazyBridgeGetter("ObjC");
registerLazyBridgeGetter("Swift");
registerLazyBridgeGetter("Java");

function registerLazyBridgeGetter(name: string) {
    Object.defineProperty(globalThis, name, {
        enumerable: true,
        configurable: true,
        get() {
            return lazyLoadBridge(name);
        }
    });
}

function lazyLoadBridge(name: string): unknown {
    send({ type: "frida:load-bridge", name });
    let bridge: unknown;
    recv("frida:bridge-loaded", message => {
        bridge = Script.evaluate(`/frida/bridges/${message.filename}`,
            "(function () { " + [
                message.source,
                `Object.defineProperty(globalThis, '${name}', { value: bridge });`,
                `return bridge;`
            ].join("\n") + " })();");
    }).wait();
    return bridge;
}

declare global {
    var REPL: REPL;
    var cm: CModule | null;
    var cs: {
        [name: string]: NativePointerValue;
    };
}

interface QuickCommandHandler {
    minArity: number;
    onInvoke: (...args: string[]) => any;
}

const rpcExports: RpcExports = {
    fridaEvaluateExpression(expression: string) {
        return evaluate(() => globalThis.eval(expression));
    },
    fridaEvaluateQuickCommand(tokens: string[]) {
        return evaluate(() => repl._invokeQuickCommand(tokens));
    },
    fridaLoadCmodule(code: string | null, toolchain: CModuleToolchain) {
        const cs = globalThis.cs;

        if (cs._frida_log === undefined)
            cs._frida_log = new NativeCallback(onLog, "void", ["pointer"]);

        let codeToLoad: string | ArrayBuffer | null = code;
        if (code === null) {
            recv("frida:cmodule-payload", (message, data) => {
                codeToLoad = data;
            });
        }

        globalThis.cm = new CModule(codeToLoad!, cs, { toolchain });
    },
};

function evaluate(func: () => any) {
    try {
        const result = func();
        if (result instanceof ArrayBuffer) {
            return result;
        }
        const type = (result === null) ? "null" : typeof result;
        const [tree, blob] = encodeValue(result, { maxDepth: 5 });
        return ["inspect", type, tree, (blob !== null) ? Array.from(new Uint8Array(blob)) : null];
    } catch (exception) {
        const e = exception as Error;
        return ["error", {
            name: e.name,
            message: e.message,
            stack: e.stack
        }];
    }
}

Object.defineProperty(rpc, "exports", {
    get() {
        return rpcExports;
    },
    set(value) {
        for (const [k, v] of Object.entries(value)) {
            rpcExports[k] = v as AnyFunction;
        }
    }
});

function onLog(messagePtr: NativePointer) {
    const message = messagePtr.readUtf8String();
    console.log(message);
}
