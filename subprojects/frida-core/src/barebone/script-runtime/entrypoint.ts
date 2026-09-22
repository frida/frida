import { Console } from "./console.js";
import { hexdump } from "./hexdump.js";
import { MessageDispatcher, MessageHandler, MessageRecvOperation } from "./message-dispatcher.js";
import { BNativePointer, BNativePointerValue, BInt64, BUInt64 } from "./primitives.js";

const messageDispatcher = new MessageDispatcher();

class Runtime {
    dispatchException(e: any): void {
        const message: ErrorMessage = {
            type: "error",
            description: "" + e
        };

        if (typeof e === "object") {
            const stack = e.stack;
            if (stack !== undefined) {
                message.stack = stack;
            }

            const fileName = e.fileName;
            if (fileName !== undefined) {
                message.fileName = fileName;
            }

            const lineNumber = e.lineNumber;
            if (lineNumber !== undefined) {
                message.lineNumber = lineNumber;
                message.columnNumber = 1;
            }
        }

        _send(JSON.stringify(message), null);
    }

    dispatchMessage(json: string, data: ArrayBuffer | null) {
        const message = JSON.parse(json);
        messageDispatcher.dispatch(message, data);
    }
}

interface ErrorMessage {
    type: "error";
    description: string;
    stack?: string;
    fileName?: string;
    lineNumber?: number;
    columnNumber?: number;
}

(Error as any).prepareStackTrace = (error: Error, stack: string) => {
    return error.toString() + "\n" + stack;
};

class NativeFunction extends Function {
    handle: BNativePointer;

    #retType: Marshaler;
    #argTypes: Marshaler[];

    constructor(address: BNativePointer, retType: NativeFunctionReturnType, argTypes: NativeFunctionArgumentType[]) {
        super();

        this.handle = address;

        this.#retType = getMarshalerFor(retType);
        this.#argTypes = argTypes.map(getMarshalerFor);

        return new Proxy(this, {
            apply(target, thiz, args) {
                return target._invoke(args);
            }
        });
    }

    _invoke(args: any[]): any {
        const nativeArgs = args.map((v, i) => this.#argTypes[i].toNative(v));
        const nativeRetval = _invoke(this.handle.$v, ...nativeArgs);
        return this.#retType.fromNative(nativeRetval);
    }
}

class NativeCallback extends BNativePointer {
    #func: (...args: any[]) => any;
    #retType: Marshaler;
    #argTypes: Marshaler[];
    #code: BNativePointer;

    constructor(
            func: (...args: any[]) => any,
            retType: NativeCallbackReturnType,
            argTypes: NativeCallbackArgumentType[]) {
        const retTypeMarshaler = getMarshalerFor(retType);
        const argTypeMarshalers = argTypes.map(getMarshalerFor);
        const code = Memory.alloc(Process.pageSize) as unknown as BNativePointer;

        super(code);

        this.#func = func;
        this.#retType = retTypeMarshaler;
        this.#argTypes = argTypeMarshalers;
        this.#code = code;

        _installNativeCallback(code.$v, this, argTypes.length);
    }

    _invoke(args: bigint[], returnAddress: NativePointer, context: CpuContext): bigint {
        const ic: UnixInvocationContext = {
            returnAddress,
            context,
            threadId: -1,
            depth: 0,
            errno: -1
        };
        const jsArgs = args.map((v, i) => this.#argTypes[i].fromNative(v));
        const jsRetval = this.#func.call(ic, ...jsArgs);
        return this.#retType.toNative(jsRetval);
    }
}

Object.defineProperties(globalThis, {
    global: {
        enumerable: false,
        value: globalThis
    },
    $rt: {
        enumerable: false,
        value: new Runtime(),
    },
    rpc: {
        enumerable: true,
        value: {
            exports: {}
        }
    },
    recv: {
        enumerable: true,
        value(): MessageRecvOperation {
            let type: string, callback: MessageHandler;
            if (arguments.length === 1) {
                type = "*";
                callback = arguments[0];
            } else {
                type = arguments[0];
                callback = arguments[1];
            }
            return messageDispatcher.subscribe(type, callback);
        }
    },
    send: {
        enumerable: true,
        value(payload: any, data: ArrayBuffer | null = null) {
            const message = {
                type: "send",
                payload
            };
            _send(JSON.stringify(message), data);
        }
    },
    ptr: {
        enumerable: true,
        value(v: string | number | bigint | BUInt64 | BInt64 | BNativePointerValue) {
            return new BNativePointer(v);
        }
    },
    NULL: {
        enumerable: true,
        value: new BNativePointer("0")
    },
    NativePointer: {
        enumerable: true,
        value: BNativePointer
    },
    int64: {
        enumerable: true,
        value(v: string | number | bigint | BInt64) {
            return new BInt64(v);
        }
    },
    Int64: {
        enumerable: true,
        value: BInt64
    },
    uint64: {
        enumerable: true,
        value(v: string | number | bigint | BUInt64) {
            return new BUInt64(v);
        }
    },
    UInt64: {
        enumerable: true,
        value: BUInt64
    },
    NativeFunction: {
        enumerable: true,
        value: NativeFunction
    },
    NativeCallback: {
        enumerable: true,
        value: NativeCallback
    },
    Module: {
        enumerable: true,
        value: {
            findExportByName(moduleName: string | null, exportName: string): NativePointer | null {
                return null;
            },
            getExportByName(moduleName: string | null, exportName: string): NativePointer {
                throw new Error(`unable to find export '${exportName}'`);
            }
        }
    },
    console: {
        enumerable: true,
        value: new Console()
    },
    hexdump: {
        enumerable: true,
        value: hexdump
    },
});

Object.defineProperties(Script, {
    runtime: {
        enumerable: true,
        value: "QJS"
    },
    setGlobalAccessHandler: {
        enumerable: true,
        value(handler: GlobalAccessHandler | null): void {
        }
    },
});

Object.defineProperties(Process, {
    id: {
        enumerable: true,
        value: 0
    },
    platform: {
        enumerable: true,
        value: "barebone"
    },
    codeSigningPolicy: {
        enumerable: true,
        value: "optional"
    },
    isDebuggerAttached: {
        enumerable: true,
        value() {
            return true;
        }
    },
    enumerateModules: {
        enumerable: true,
        value(): Module[] {
            return [
                {
                    name: "kernel",
                    base: NULL,
                    size: 0x1000,
                    path: "/kernel",
                    ensureInitialized() {
                    },
                    enumerateImports() {
                        return [];
                    },
                    enumerateExports() {
                        return [];
                    },
                    enumerateSymbols() {
                        return [];
                    },
                    enumerateRanges() {
                        return [];
                    },
                    enumerateSections() {
                        return [];
                    },
                    enumerateDependencies() {
                        return [];
                    },
                    findExportByName() {
                        return null;
                    },
                    getExportByName(name) {
                        throw new Error(`unable to find export '${name}'`);
                    },
                    findSymbolByName() {
                        return null;
                    },
                    getSymbolByName(name) {
                        throw new Error(`unable to find symbol '${name}'`);
                    }
                }
            ];
        }
    },
    setExceptionHandler: {
        enumerable: true,
        value(callback: ExceptionHandlerCallback): void {
        }
    },
});

const marshalers: { [type: string]: Marshaler } = {
    void: {
        fromNative(v) {
        },
        toNative(v) {
            throw new Error("invalid operation");
        }
    },
    pointer: {
        fromNative(v) {
            return new BNativePointer(v);
        },
        toNative(v) {
            if (typeof v !== "object" || v === null) {
                throw new Error("expected a pointer");
            }

            if (v instanceof BNativePointer) {
                return v.$v;
            }

            const handle = v.handle;
            if (handle === undefined || !(handle instanceof BNativePointer)) {
                throw new Error("expected a pointer");
            }
            return handle.$v;
        }
    },
    int: {
        fromNative(v) {
            if ((v & 0x80000000n) !== 0n) {
                return Number(-(0xffffffffn - v + 1n));
            }
            return Number(v);
        },
        toNative(v) {
            if (typeof v !== "number") {
                throw new Error("expected an integer");
            }
            return BigInt(v);
        }
    },
    uint: {
        fromNative(v) {
            return Number(v);
        },
        toNative(v) {
            if (typeof v !== "number") {
                throw new Error("expected an integer");
            }
            return BigInt(v);
        }
    },
    size_t: {
        fromNative(v) {
            return new BUInt64(v);
        },
        toNative(v) {
            if (typeof v === "number") {
                return BigInt(v);
            }
            return v.$v;
        }
    },
};

function getMarshalerFor(t: NativeFunctionReturnType | NativeFunctionArgumentType): Marshaler {
    const m = marshalers[t as string];
    if (m === undefined) {
        throw new Error(`Type ${t} is not yet supported`);
    }
    return m;
}

interface Marshaler {
    fromNative(v: bigint): any;
    toNative(v: any): bigint;
}

declare function _send(json: string, data: ArrayBuffer | null): void;
declare function _invoke(impl: bigint, ...args: bigint[]): bigint;
declare function _installNativeCallback(code: bigint, wrapper: NativeCallback, arity: number): void;
