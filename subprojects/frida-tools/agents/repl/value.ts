// Turn an arbitrary value into a bounded, JSON-safe tagged tree (plus a binary
// blob holding any byte buffers), so that serializing it can never hang or blow
// up on cyclic or very large objects such as a ControlFlowGraph. The REPL host
// decodes and pretty-prints the tree.

export interface EncodeOptions {
    maxDepth?: number;
}

const DEFAULT_MAX_DEPTH = 3;

export type PackedValue = [EncodedValueTree, ArrayBuffer | null];

export type EncodedValueTree = EncodedValue;
type EncodedValue = [ValueTag, ...any[]];

type BytesEncoded = [tag: ValueTag.Bytes, offset: number, length: number, kind: BytesKind];
type BytesKind = "ArrayBuffer" | "DataView" | TypedArrayName;

const enum ValueTag {
    Number = 0,
    String = 1,
    Object = 2,
    Array = 3,
    NativePointer = 4,
    Null = 5,
    Boolean = 6,
    Bytes = 7,
    Function = 8,
    Error = 9,
    Undefined = 10,
    BigInt = 11,
    Symbol = 12,
    Date = 13,
    RegExp = 14,
    Map = 15,
    Set = 16,
    Promise = 17,
    WeakMap = 18,
    WeakSet = 19,
    DepthLimit = 20,
    Circular = 21,
}

type TypedArrayName =
    | "Int8Array"
    | "Uint8Array"
    | "Uint8ClampedArray"
    | "Int16Array"
    | "Uint16Array"
    | "Int32Array"
    | "Uint32Array"
    | "Float32Array"
    | "Float64Array"
    | "BigInt64Array"
    | "BigUint64Array";

interface EncodeState {
    seen: Map<unknown, number>;
    nextId: number;
}

export function encodeValue(value: unknown, options?: EncodeOptions): PackedValue {
    const chunks: BinaryChunk[] = [];
    const maxDepth = options?.maxDepth ?? DEFAULT_MAX_DEPTH;
    const state = makeEncodeState();
    const tree = collectTree(value, chunks, maxDepth, 0, state);
    const { blob, offsets } = packChunks(chunks);
    const patchedTree = patchTree(tree, offsets);
    return [patchedTree, blob];
}

function makeEncodeState(): EncodeState {
    return {
        seen: new Map(),
        nextId: 1
    };
}

function collectTree(
    value: unknown,
    chunks: BinaryChunk[],
    maxDepth: number,
    depth: number,
    state: EncodeState
): EncodedValueTree {
    if (value === null) {
        return [ValueTag.Null];
    }

    const t = typeof value;

    if (t === "number") {
        return [ValueTag.Number, value as number];
    }

    if (t === "string") {
        return [ValueTag.String, value as string];
    }

    if (t === "object") {
        const obj = value as any;

        if (obj instanceof NativePointer) {
            return [ValueTag.NativePointer, (obj as NativePointer).toString()];
        }

        if (isBinaryLike(obj)) {
            const index = chunks.length;
            const binary = obj as ArrayBufferView | ArrayBuffer;
            const kind = getBytesKind(binary);
            chunks.push({ index, buf: binary });
            return [ValueTag.Bytes, index, kind];
        }

        if (obj instanceof Error) {
            const err = obj as Error;
            const name = err.name ?? "Error";
            const message = err.message ?? "";
            const stack = err.stack ?? "";
            return [ValueTag.Error, name, message, stack];
        }

        if (obj instanceof Date) {
            return [ValueTag.Date, (obj as Date).toISOString()];
        }

        if (obj instanceof RegExp) {
            const r = obj as RegExp;
            return [ValueTag.RegExp, r.source, r.flags];
        }

        if (obj instanceof Promise) {
            return [ValueTag.Promise];
        }

        if (obj instanceof WeakMap) {
            return [ValueTag.WeakMap];
        }

        if (obj instanceof WeakSet) {
            return [ValueTag.WeakSet];
        }

        const isArray = Array.isArray(obj);
        const isMap = obj instanceof Map;
        const isSet = obj instanceof Set;

        const existingId = state.seen.get(obj);
        if (existingId !== undefined) {
            return [ValueTag.Circular, existingId];
        }

        if (depth >= maxDepth) {
            if (isArray) {
                return [ValueTag.DepthLimit, ValueTag.Array];
            }
            if (isMap) {
                return [ValueTag.DepthLimit, ValueTag.Map];
            }
            if (isSet) {
                return [ValueTag.DepthLimit, ValueTag.Set];
            }
            return [ValueTag.DepthLimit, ValueTag.Object];
        }

        const id = state.nextId++;
        state.seen.set(obj, id);

        if (isMap) {
            const mapValue = obj as Map<unknown, unknown>;
            const entries: [EncodedValueTree, EncodedValueTree][] = [];
            for (const [k, v] of mapValue.entries()) {
                const keyTree = collectTree(k, chunks, maxDepth, depth + 1, state);
                const valueTree = collectTree(v, chunks, maxDepth, depth + 1, state);
                entries.push([keyTree, valueTree]);
            }
            return [ValueTag.Map, id, entries];
        }

        if (isSet) {
            const setValue = obj as Set<unknown>;
            const items: EncodedValueTree[] = [];
            for (const v of setValue.values()) {
                items.push(collectTree(v, chunks, maxDepth, depth + 1, state));
            }
            return [ValueTag.Set, id, items];
        }

        if (isArray) {
            const elements = (obj as unknown[]).map(v =>
                collectTree(v, chunks, maxDepth, depth + 1, state),
            );
            return [ValueTag.Array, id, elements];
        }

        try {
            const maybeToJSON = obj.toJSON;
            if (typeof maybeToJSON === "function") {
                const converted = maybeToJSON.call(obj);
                return collectTree(converted, chunks, maxDepth, depth, state);
            }
        } catch {
        }

        const entries: [EncodedValueTree, EncodedValueTree][] = [];

        for (const k of Object.keys(obj as Record<string, unknown>)) {
            const keyTree: EncodedValueTree = [ValueTag.String, k];
            let v;
            try {
                v = obj[k];
            } catch {
                continue;
            }
            const valueTree = collectTree(v, chunks, maxDepth, depth + 1, state);
            entries.push([keyTree, valueTree]);
        }

        const symbols = Object.getOwnPropertySymbols(obj as object);
        for (const s of symbols) {
            if (!Object.prototype.propertyIsEnumerable.call(obj, s)) {
                continue;
            }
            const keyTree: EncodedValueTree = [ValueTag.Symbol, String(s)];
            let v;
            try {
                v = obj[s];
            } catch {
                continue;
            }
            const valueTree = collectTree(v, chunks, maxDepth, depth + 1, state);
            entries.push([keyTree, valueTree]);
        }

        return [ValueTag.Object, id, entries];
    }

    if (t === "boolean") {
        return [ValueTag.Boolean, value as boolean];
    }

    if (t === "function") {
        const fn = value as Function;
        const name = fn.name ?? "";
        const sig = name === "" ? "[Function]" : `[Function: ${name}]`;
        return [ValueTag.Function, sig];
    }

    if (t === "undefined") {
        return [ValueTag.Undefined];
    }

    if (t === "bigint") {
        const big = value as bigint;
        return [ValueTag.BigInt, big.toString()];
    }

    if (t === "symbol") {
        return [ValueTag.Symbol, String(value)];
    }

    return [ValueTag.Undefined];
}

interface BinaryChunk {
    index: number;
    buf: ArrayBufferView | ArrayBuffer;
}

function packChunks(chunks: BinaryChunk[]): {
    blob: ArrayBuffer | null;
    offsets: { offset: number; length: number }[];
} {
    if (chunks.length === 0) {
        return { blob: null, offsets: [] };
    }

    let total = 0;
    const lengths = chunks.map(({ buf }) => {
        const b = buf instanceof ArrayBuffer ? buf : buf.buffer;
        return b.byteLength;
    });
    for (const len of lengths) {
        total += len;
    }

    const blob = new Uint8Array(total);
    const offsets: { offset: number; length: number }[] = [];

    let offset = 0;
    chunks.forEach(({ buf }, index) => {
        const raw = buf instanceof ArrayBuffer
            ? new Uint8Array(buf)
            : new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
        blob.set(raw, offset);
        offsets[index] = { offset, length: raw.byteLength };
        offset += raw.byteLength;
    });

    return { blob: blob.buffer as ArrayBuffer, offsets };
}

function patchTree(
    tree: EncodedValueTree,
    offsets: { offset: number; length: number }[]
): EncodedValueTree {
    const [tag, ...rest] = tree;

    switch (tag) {
        case ValueTag.Object: {
            const id = rest[0] as number;
            const entries = rest[1] as [EncodedValueTree, EncodedValueTree][];
            const patchedEntries = entries.map(([k, v]) => [
                patchTree(k, offsets),
                patchTree(v, offsets),
            ] as [EncodedValueTree, EncodedValueTree]);
            return [ValueTag.Object, id, patchedEntries];
        }

        case ValueTag.Array: {
            const id = rest[0] as number;
            const items = (rest[1] as EncodedValueTree[])
                .map(child => patchTree(child, offsets));
            return [ValueTag.Array, id, items];
        }

        case ValueTag.Bytes: {
            const index = rest[0] as number;
            const kind = rest[1] as BytesKind;
            const { offset, length } = offsets[index];
            return [ValueTag.Bytes, offset, length, kind] as BytesEncoded;
        }

        case ValueTag.Map: {
            const id = rest[0] as number;
            const entries = rest[1] as [EncodedValueTree, EncodedValueTree][];
            const patchedEntries = entries.map(([k, v]) => [
                patchTree(k, offsets),
                patchTree(v, offsets),
            ] as [EncodedValueTree, EncodedValueTree]);
            return [ValueTag.Map, id, patchedEntries];
        }

        case ValueTag.Set: {
            const id = rest[0] as number;
            const items = rest[1] as EncodedValueTree[];
            const patchedItems = items.map(child => patchTree(child, offsets));
            return [ValueTag.Set, id, patchedItems];
        }

        default:
            return tree;
    }
}

function getBytesKind(v: ArrayBufferView | ArrayBuffer): BytesKind {
    if (v instanceof ArrayBuffer) {
        return "ArrayBuffer";
    }
    if (v instanceof DataView) {
        return "DataView";
    }
    const ctor = (v as { constructor?: { name?: string } }).constructor;
    const name = ctor?.name ?? "ArrayBuffer";
    return name as BytesKind;
}

function isBinaryLike(v: unknown): v is ArrayBufferView | ArrayBuffer {
    return v instanceof ArrayBuffer || ArrayBuffer.isView(v);
}
