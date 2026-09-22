export class MessageDispatcher {
    #messages: MessageItem[] = [];
    #operations = new Map<string, MessageHandler[]>();

    dispatch(message: any, data: ArrayBuffer | null) {
        if (message instanceof Array && message[0] === "frida:rpc") {
            this.#handleRpcMessage(message[1], message[2], message.slice(3));
        } else {
            this.#messages.push([message, data]);
            this.#dispatchMessages();
        }
    }

    subscribe(type: string, handler: MessageHandler): MessageRecvOperation {
        const op = new MessageRecvOperation(handler);

        const ops = this.#operations;
        let opsForType = ops.get(type);
        if (opsForType === undefined) {
            opsForType = [];
            ops.set(type, opsForType);
        }
        opsForType.push(op.handler);

        this.#dispatchMessages();

        return op;
    }

    #handleRpcMessage(id: number, operation: RpcMessageType, params: any[]) {
        const exports = rpc.exports;

        if (operation === "call") {
            const method = params[0];
            const args = params[1];

            if (!exports.hasOwnProperty(method)) {
                this.#reply(id, "error", `unable to find method "${method}"`);
                return;
            }

            try {
                const result = exports[method].apply(exports, args);
                if (typeof result === "object" && result !== null &&
                    typeof result.then === "function") {
                    result
                        .then((value: any) => {
                            this.#reply(id, "ok", value);
                        })
                        .catch((error: any) => {
                            this.#reply(id, "error", error.message, [error.name, error.stack, error]);
                        });
                } else {
                    this.#reply(id, "ok", result);
                }
            } catch (e: any) {
                this.#reply(id, "error", e.message, [e.name, e.stack, e]);
            }
        } else if (operation === "list") {
            this.#reply(id, "ok", Object.keys(exports));
        }
    }

    #reply(id: number, type: RpcMessageType, result: any, params: any[] = []) {
        if (result instanceof ArrayBuffer)
            send(["frida:rpc", id, type, {}].concat(params), result);
        else
            send(["frida:rpc", id, type, result].concat(params));
    }

    #dispatchMessages() {
        this.#messages.splice(0).forEach(this.#dispatch);
    }

    #dispatch = (item: MessageItem) => {
        const [message, data] = item;

        const ops = this.#operations;

        let opsForType: MessageHandler[] | undefined;

        let handlerType: string | undefined = message.type;
        if (handlerType !== undefined) {
            opsForType = ops.get(handlerType);
        }

        if (opsForType === undefined) {
            handlerType = "*";
            opsForType = ops.get(handlerType);
        }

        if (opsForType === undefined) {
            this.#messages.push(item);
            return;
        }

        const complete = opsForType.shift()!;
        if (opsForType.length === 0)
            ops.delete(handlerType!);

        complete(message, data);
    };
}

type MessageItem = [message: any, data: ArrayBuffer | null];
export type MessageHandler = (message: any, data: ArrayBuffer | null) => void;
type RpcMessageType = "call" | "list" | "ok" | "error";

export class MessageRecvOperation {
    handler: MessageHandler;
    #completed = false;

    constructor(handler: MessageHandler) {
        this.handler = handler;
    }

    wait() {
        while (!this.#completed)
            _waitForEvent();
    }

    _complete(message: any, data: ArrayBuffer | null) {
        try {
            this.handler(message, data);
        } finally {
            this.#completed = true;
        }
    }
}

declare function _waitForEvent(): void;
