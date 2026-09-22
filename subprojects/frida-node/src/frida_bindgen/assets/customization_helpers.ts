const STANDARD_SPAWN_OPTION_NAMES = new Set<keyof SpawnOptions>([
    "argv",
    "envp",
    "env",
    "cwd",
    "stdio",
]);

interface LogMessage {
    type: "log";
    level: LogLevel;
    payload: string;
}

class ScriptServices implements RpcController {
    exportsProxy = new ScriptExportsProxy(this);

    #script: Script;
    #pendingRequests = new Map<number, (error: Error | null, result?: any) => void>();
    #nextRequestId = 1;

    constructor(script: Script) {
        this.#script = script;
        process.nextTick(() => {
            script.message.connect(() => {});
        });
    }

    handleMessageIntercept = (message: Message, data: Buffer | null): boolean => {
        if (message.type === MessageType.Send && isRpcSendMessage(message)) {
            const [ , id, operation, ...params ] = message.payload;
            this.#onRpcMessage(id, operation, params, data);
            return false;
        } else if (isLogMessage(message)) {
            const opaqueMessage: any = message;
            const logMessage: LogMessage = opaqueMessage;
            this.#script.logHandler(logMessage.level, logMessage.payload);
            return false;
        }

        return true;
    };

    request(operation: string, params: any[], data: Buffer | null, cancellable: Cancellable | null = null): Promise<any> {
        return new Promise((resolve, reject) => {
            const id = this.#nextRequestId++;

            const complete = (error: Error | null, result?: any): void => {
                if (cancellable !== null) {
                    cancellable.cancelled.disconnect(onOperationCancelled);
                }
                this.#script.destroyed.disconnect(onScriptDestroyed);

                this.#pendingRequests.delete(id);

                if (error === null) {
                    resolve(result);
                } else {
                    reject(error);
                }
            };

            function onScriptDestroyed(): void {
                complete(new Error("Script is destroyed"));
            }

            function onOperationCancelled(): void {
                complete(new Error("Operation was cancelled"));
            }

            this.#pendingRequests.set(id, complete);

            this.#script.post(["frida:rpc", id, operation, ...params], data);
            this.#script.destroyed.connect(onScriptDestroyed);
            if (cancellable !== null) {
                cancellable.cancelled.connect(onOperationCancelled);
                if (cancellable.isCancelled) {
                    onOperationCancelled();
                    return;
                }
            }
            if (this.#script.isDestroyed) {
                onScriptDestroyed();
            }
        });
    }

    #onRpcMessage(id: number, operation: RpcOperation, params: any[], data: Buffer | null) {
        if (operation === RpcOperation.Ok || operation === RpcOperation.Error) {
            const callback = this.#pendingRequests.get(id);
            if (callback === undefined) {
                return;
            }

            let value = null;
            let error = null;
            if (operation === RpcOperation.Ok) {
                if (data !== null) {
                    value = (params.length > 1) ? [params[1], data] : data;
                } else {
                    value = params[0];
                }
            } else {
                const [message, name, stack, rawErr] = params;
                error = new Error(message);
                error.name = name;
                error.stack = stack;
                Object.assign(error, rawErr);
            }

            callback(error, value);
        }
    }
}

class ScriptExportsProxy implements ScriptExports {
    [name: string]: (...args: any[]) => Promise<any>;

    constructor(rpcController: RpcController) {
        return new Proxy(this, {
            has(target, property) {
                return !isReservedMethodName(property);
            },
            get(target, property, receiver) {
                if (typeof property === "symbol") {
                    if (property === inspect.custom) {
                        return inspectProxy;
                    }

                    return undefined;
                }

                if (property in target) {
                    return target[property];
                }

                if (isReservedMethodName(property)) {
                    return undefined;
                }

                return (...args: any[]): Promise<any> => {
                    let cancellable: Cancellable | null = null;
                    if (args[args.length - 1] instanceof Cancellable) {
                        cancellable = args.pop();
                    }

                    let data: Buffer | null = null;
                    if (Buffer.isBuffer(args[args.length - 1])) {
                        data = args.pop();
                    }

                    return rpcController.request("call", [property, args], data, cancellable);
                };
            },
            set(target, property, value, receiver) {
                if (typeof property === "symbol") {
                    return false;
                }
                target[property] = value;
                return true;
            },
            ownKeys(target) {
                return Object.getOwnPropertyNames(target);
            },
            getOwnPropertyDescriptor(target, property) {
                if (property in target) {
                    return Object.getOwnPropertyDescriptor(target, property);
                }

                if (isReservedMethodName(property)) {
                    return undefined;
                }

                return {
                    writable: true,
                    configurable: true,
                    enumerable: true
                };
            },
        });
    }
}

function inspectProxy() {
    return "ScriptExportsProxy {}";
}

interface RpcController {
    request(operation: string, params: any[], data: ArrayBuffer | null, cancellable: Cancellable | null): Promise<any>;
}

enum RpcOperation {
    Ok = "ok",
    Error = "error"
}

function isInternalMessage(message: Message): boolean {
    return isRpcMessage(message) || isLogMessage(message);
}

function isRpcMessage(message: Message): boolean {
    return message.type === MessageType.Send && isRpcSendMessage(message);
}

function isRpcSendMessage(message: SendMessage): boolean {
    const payload = message.payload;
    if (!Array.isArray(payload)) {
        return false;
    }

    return payload[0] === "frida:rpc";
}

function isLogMessage(message: Message): boolean {
    return message.type as string === "log";
}

function log(level: LogLevel, text: string): void {
    switch (level) {
        case LogLevel.Info:
            console.log(text);
            break;
        case LogLevel.Warning:
            console.warn(text);
            break;
        case LogLevel.Error:
            console.error(text);
            break;
    }
}

const reservedMethodNames = new Set<string>([
    "then",
    "catch",
    "finally",
]);

function isReservedMethodName(name: string | number | symbol): boolean {
    return reservedMethodNames.has(name.toString());
}

const IO_PRIORITY_DEFAULT = 0;

class IOStreamAdapter extends Duplex {
    #impl: IOStream;
    #input: InputStream;
    #output: OutputStream;
    #pending = new Set<Promise<void>>();

    #cancellable = new Cancellable();

    constructor(impl: IOStream) {
        super({});

        this.#impl = impl;
        this.#input = impl.inputStream;
        this.#output = impl.outputStream;
    }

    async _destroy(error: Error | null, callback: (error: Error | null) => void): Promise<void> {
        this.#cancellable.cancel();

        for (const operation of this.#pending) {
            try {
                await operation;
            } catch (e) {
            }
        }

        try {
            await this.#impl.close(IO_PRIORITY_DEFAULT);
        } catch (e) {
        }

        callback(error);
    }

    _read(size: number): void {
        const operation = this.#input.read(size, IO_PRIORITY_DEFAULT, this.#cancellable)
            .then((data: Buffer): void => {
                const isEof = data.length === 0;
                if (isEof) {
                    this.push(null);
                    return;
                }

                this.push(data);
            })
            .catch((error: Error): void => {
                if (this.#impl.closed) {
                    this.push(null);
                }
                this.emit("error", error);
            });
        this.#track(operation);
    }

    _write(chunk: any, encoding: BufferEncoding, callback: (error?: Error | null) => void): void {
        let data: Buffer;
        if (Buffer.isBuffer(chunk)) {
            data = chunk;
        } else {
            data = Buffer.from(chunk, encoding);
        }
        const operation = this.#writeAll(data)
            .then((): void => {
                callback(null);
            })
            .catch((error: Error): void => {
                callback(error);
            });
        this.#track(operation);
    }

    async #writeAll(data: Buffer): Promise<void> {
        let offset = 0;
        do {
            const n = await this.#output.write(data.slice(offset), IO_PRIORITY_DEFAULT, this.#cancellable);
            offset += n;
        } while (offset !== data.length);
    }

    #track(operation: Promise<void>): void {
        this.#pending.add(operation);
        operation
            .catch(_ => {})
            .finally(() => {
                this.#pending.delete(operation);
            });
    }
}

class CallbackAuthenticationService extends binding.AbstractAuthenticationService {
    #callback: AuthenticationCallback;

    constructor(callback: AuthenticationCallback) {
        super();
        this.#callback = callback;
    }

    async authenticate(token: string, cancellable: Cancellable | null): Promise<string> {
        const info = await this.#callback(token);
        return JSON.stringify(info);
    }
}

function parseSocketAddress(address: BaseSocketAddress): SocketAddress {
    const family = address.family;
    switch (family) {
        case SocketFamily.Unix: {
            const addr = address as UnixSocketAddress;
            switch (addr.addressType) {
                case UnixSocketAddressType.Anonymous:
                    return {
                        family: "unix:anonymous",
                    };
                case UnixSocketAddressType.Path:
                    return {
                        family: "unix:path",
                        path: addr.path.toString(),
                    };
                case UnixSocketAddressType.Abstract:
                case UnixSocketAddressType.AbstractPadded:
                    return {
                        family: "unix:abstract",
                        path: addr.path,
                    };
            }
            break;
        }
        case SocketFamily.Ipv4: {
            const addr = address as InetSocketAddress;
            return {
                family: "ipv4",
                address: addr.address.toString(),
                port: addr.port,
            };
        }
        case SocketFamily.Ipv6: {
            const addr = address as InetSocketAddress;
            return {
                family: "ipv6",
                address: addr.address.toString(),
                port: addr.port,
                flowlabel: addr.flowinfo,
                scopeid: addr.scopeId,
            };
        }
    }

    throw new Error("invalid BaseSocketAddress");
}

function objectToStrv(object: { [name: string]: string }): string[] {
    return Object.entries(object).map(([k, v]) => `${k}=${v}`);
}
