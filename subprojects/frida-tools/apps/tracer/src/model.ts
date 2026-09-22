import { useR2, type Platform, type Architecture } from "@frida/react-use-r2";
import { OverlayToaster } from "@blueprintjs/core";
import { useCallback, useEffect, useRef, useState } from "react";
import useWebSocket, { ReadyState } from "react-use-websocket";

const SOCKET_URL = (import.meta.env.MODE === "development")
    ? inferDevModeHost()
    : `ws://${window.location.host}`;

export function useModel() {
    const { sendJsonMessage, lastJsonMessage, readyState } = useWebSocket<TracerMessage>(SOCKET_URL);
    const rpcStateRef = useRef<RpcState>({
        pendingRequests: new Map<RequestId, ResponseCallbacks<any>>(),
        nextRequestId: 1,
    });

    const lostConnection = readyState === ReadyState.CLOSED;

    const [spawnedProgram, setSpawnedProgram] = useState<string | null>(null);
    const [process, setProcess] = useState<ProcessDetails | null>(null);

    const [handlers, setHandlers] = useState<Handler[]>([]);
    const [selectedScope, setSelectedScope] = useState<ScopeId>("");
    const [selectedHandlerId, _setSelectedHandlerId] = useState<HandlerId | null>(null);
    const [selectedHandler, setSelectedHandler] = useState<Handler | null>(null);
    const [handlerCode, setHandlerCode] = useState("");
    const [draftedCode, setDraftedCode] = useState("");
    const [handlerMuted, _setHandlerMuted] = useState(false);
    const [captureBacktraces, _setCaptureBacktraces] = useState(false);

    const [selectedTab, _setSelectedTab] = useState<TabId>("events");

    const [events, setEvents] = useState<Event[]>([]);
    const [latestMatchingEventIndex, setLatestMatchingEventIndex] = useState<number | null>(null);
    const [selectedEventIndex, _setSelectedEventIndex] = useState<number | null>(null);

    const [disassemblyTarget, setDisassemblyTarget] = useState<DisassemblyTarget>();
    const [memoryLocation, setMemoryLocation] = useState<bigint | undefined>();

    const [addingTargets, setAddingTargets] = useState(false);
    const [stagedItems, setStagedItems] = useState<StagedItem[]>([]);

    const cachedSymbolsRef = useRef(new Map<bigint, string>());

    const pushState = useCallback((updates: Partial<AppNavigationState>) => {
        const state: AppNavigationState = {
            handler: selectedHandlerId,
            tab: selectedTab,
            eventIndex: selectedEventIndex,
            disassemblyTarget,
            memoryLocation,
            ...updates,
        };
        history.pushState(state, "");
    }, [selectedHandlerId, selectedTab, selectedEventIndex, disassemblyTarget, memoryLocation]);

    const request = useCallback(<T extends RequestType>(type: T, payload: RequestPayload[T]): Promise<ResponsePayload[T]> => {
        const rpcState = rpcStateRef.current;
        const id = rpcState.nextRequestId++;
        return new Promise<ResponsePayload[T]>((resolve, reject) => {
            rpcState.pendingRequests.set(id, { resolve, reject });
            sendJsonMessage({ type, id, payload });
        });
    }, [sendJsonMessage]);

    const onR2ReadRequest = useCallback(async (address: bigint, size: number) => {
        const result = await request("memory:read", {
            address: "0x" + address.toString(16),
            size
        });
        return (result !== null) ? new Uint8Array(result) : null;
    }, [request]);

    useR2({
        source: (process !== null)
            ? {
                platform: process.platform,
                arch: process.arch,
                pointerSize: process.pointer_size,
                pageSize: process.page_size,
                onReadRequest: onR2ReadRequest
            }
            : undefined,
    });

    const respawn = useCallback(async () => {
        await request("tracer:respawn", {});
    }, [request]);

    const selectScope = useCallback((id: ScopeId) => {
        setSelectedScope(id);
    }, []);

    useEffect(() => {
        const id = selectedHandlerId;
        if (id === null) {
            setSelectedScope("");
            setSelectedHandler(null);
            _setHandlerMuted(false);
            _setCaptureBacktraces(false);
            return;
        }

        const handler = handlers.find(h => h.id === id)!;
        setSelectedScope(handler.scope);
        setSelectedHandler(handler);
        const { config } = handler;
        _setHandlerMuted(config.muted);
        _setCaptureBacktraces(config.capture_backtraces);
    }, [selectedHandlerId, handlers, request]);

    useEffect(() => {
        setHandlerCode("");
        setDraftedCode("");

        const id = selectedHandlerId;
        if (id === null) {
            return;
        }

        let ignore = false;

        async function loadCode() {
            const { code } = await request("handler:load", { id: id! });
            if (!ignore) {
                setHandlerCode(code);
                setDraftedCode(code);
            }
        }

        loadCode();

        return () => {
            ignore = true;
        };
    }, [selectedHandlerId, request]);

    const deployCode = useCallback(async (code: string) => {
        setHandlerCode(code);
        setDraftedCode(code);
        await request("handler:save", { id: selectedHandler!.id, code });
    }, [request, selectedHandler]);

    const setSelectedHandlerId = useCallback((handler: HandlerId) => {
        _setSelectedHandlerId(handler);
        pushState({ handler });
    }, [pushState]);

    const setHandlerMuted = useCallback(async (muted: boolean) => {
        setHandlers(updateHandlerConfig(selectedHandlerId!, { muted }, handlers));
        _setHandlerMuted(muted);
        await request("handler:configure", {
            id: selectedHandlerId!,
            parameters: { muted }
        });
    }, [request, selectedHandler]);

    const setCaptureBacktraces = useCallback(async (enabled: boolean) => {
        setHandlers(updateHandlerConfig(selectedHandlerId!, { capture_backtraces: enabled }, handlers));
        _setCaptureBacktraces(enabled);
        await request("handler:configure", {
            id: selectedHandler!.id,
            parameters: { capture_backtraces: enabled }
        });
    }, [request, selectedHandler]);

    const disassemble = useCallback((target: DisassemblyTarget) => {
        _setSelectedTab("disassembly");
        setDisassemblyTarget(target);
        pushState({ tab: "disassembly", disassemblyTarget: target });
    }, [pushState]);

    const showMemoryLocation = useCallback((location: bigint) => {
        _setSelectedTab("memory");
        setMemoryLocation(location);
        pushState({ tab: "memory", memoryLocation: location });
    }, [pushState]);

    const startAddingTargets = useCallback(() => {
        setAddingTargets(true);
    }, []);

    const finishAddingTargets = useCallback(() => {
        setAddingTargets(false);
        setStagedItems([]);
    }, []);

    const stageItems = useCallback(async (scope: TraceSpecScope, query: string) => {
        if (query.length === 0 || query === "*") {
            return;
        }

        const { items } = await request("targets:stage", {
            profile: {
                spec: [
                    ["include", scope, query],
                ]
            }
        });
        setStagedItems(items);
    }, [request]);

    const commitItems = useCallback(async (id: StagedItemId | null) => {
        finishAddingTargets();
        await request("targets:commit", { id });
    }, [finishAddingTargets, request]);

    const addInstructionHook = useCallback(async (address: bigint) => {
        await request("targets:stage", {
            profile: {
                spec: [
                    ["include", TraceSpecScope.AbsoluteInstruction, "0x" + address.toString(16)],
                ]
            }
        });
        const { ids, errors } = await request("targets:commit", { id: null });
        if (ids.length === 0) {
            return;
        }
        if (errors.length !== 0) {
            const toaster = await OverlayToaster.createAsync({ position: "top" });
            toaster.show({
                intent: "danger",
                icon: "error",
                message: "Failed to add instruction hook: " + errors[0].message
            });
            return;
        }
        setSelectedHandlerId(ids[0]);
    }, [request]);

    const symbolicate = useCallback(async (addresses: bigint[]): Promise<string[]> => {
        const cache = cachedSymbolsRef.current;

        const result = addresses.map(address => cache.get(address) ?? null);

        const missingIndices = result.reduce((acc, element, i) => {
            if (element === null) {
                acc.push(i);
            }
            return acc;
        }, [] as number[]);
        if (missingIndices.length !== 0) {
            const missingAddresses = missingIndices.map(i => addresses[i]);
            const { names } = await request("symbols:resolve-addresses", {
                addresses: missingAddresses.map(addr => "0x" + addr.toString(16))
            });
            names.forEach((name, i) => {
                cache.set(missingAddresses[i], name);
                result[missingIndices[i]] = name;
            });
        }

        return result as string[];
    }, [request]);

    useEffect(() => {
        if (lastJsonMessage === null) {
            return;
        }

        switch (lastJsonMessage.type) {
            case "tracer:sync":
                setSpawnedProgram(lastJsonMessage.spawned_program);
                setProcess(lastJsonMessage.process);
                setHandlers(lastJsonMessage.handlers);
                break;
            case "handlers:add":
                setHandlers(handlers.concat(lastJsonMessage.handlers));
                break;
            case "events:add":
                setEvents(events.concat(lastJsonMessage.events));
                break;
            case "request:result":
            case "request:error":
                const { id, payload } = lastJsonMessage;
                const pendingRequests = rpcStateRef.current.pendingRequests;

                const entry = pendingRequests.get(id);
                if (entry === undefined) {
                    return;
                }
                pendingRequests.delete(id);

                if (lastJsonMessage.type === "request:result") {
                    entry.resolve(payload);
                } else {
                    const e = new Error(payload.message);
                    e.stack = payload.stack;
                    entry.reject(e);
                }

                break;
            default:
                console.log("TODO:", lastJsonMessage);
                break;
        }
    }, [lastJsonMessage]);

    const setSelectedTabId = useCallback((tab: TabId) => {
        _setSelectedTab(tab);
        pushState({ tab });
    }, [pushState]);

    useEffect(() => {
        if (selectedHandler !== null) {
            const selectedHandlerId = selectedHandler.id;
            for (let i = events.length - 1; i !== -1; i--) {
                const event = events[i];
                if (event[0] === selectedHandlerId) {
                    setLatestMatchingEventIndex(i);
                    return;
                }
            }
        }
        setLatestMatchingEventIndex(null);
    }, [selectedHandler, events]);

    const setSelectedEventIndex = useCallback((index: number | null) => {
        _setSelectedTab("events");
        _setSelectedEventIndex(index);
    }, []);

    useEffect(() => {
        function onPopState(event: PopStateEvent) {
            const state: AppNavigationState | null = event.state;
            if (state !== null) {
                _setSelectedHandlerId(state.handler);
                _setSelectedTab(state.tab);
                _setSelectedEventIndex(state.eventIndex);
                setDisassemblyTarget(state.disassemblyTarget);
                setMemoryLocation(state.memoryLocation);
            } else {
                _setSelectedHandlerId(null);
                _setSelectedTab("events");
                _setSelectedEventIndex(null);
                setDisassemblyTarget(undefined);
                setMemoryLocation(undefined);
            }
        }

        addEventListener("popstate", onPopState);

        return () => {
            removeEventListener("popstate", onPopState);
        };
    });

    return {
        lostConnection,

        spawnedProgram,
        respawn,
        process,

        handlers,
        selectedScope,
        selectScope,
        selectedHandler,
        setSelectedHandlerId,
        handlerCode,
        draftedCode,
        setDraftedCode,
        deployCode,
        handlerMuted,
        setHandlerMuted,
        captureBacktraces,
        setCaptureBacktraces,

        selectedTabId: selectedTab,
        setSelectedTabId,

        events,
        latestMatchingEventIndex,
        selectedEventIndex,
        setSelectedEventIndex,

        disassemblyTarget,
        disassemble,

        memoryLocation,
        showMemoryLocation,

        addingTargets,
        startAddingTargets,
        finishAddingTargets,
        stageItems,
        stagedItems,
        commitItems,

        addInstructionHook,

        symbolicate,
    };
}

function updateHandlerConfig(id: HandlerId, updates: Partial<HandlerConfig>, handlers: Handler[]): Handler[] {
    return handlers.map(h => {
        if (h.id === id) {
            return {
                ...h,
                config: {
                    ...h.config,
                    ...updates
                }
            };
        }
        return h;
    });
}

type TraceSpec = TraceSpecItem[];
type TraceSpecItem = [TraceSpecOperation, TraceSpecScope, TraceSpecPattern];
type TraceSpecOperation = "include" | "exclude";
export enum TraceSpecScope {
    Function = "function",
    RelativeFunction = "relative-function",
    AbsoluteInstruction = "absolute-instruction",
    Imports = "imports",
    Module = "module",
    ObjcMethod = "objc-method",
    SwiftFunc = "swift-func",
    JavaMethod = "java-method",
    DebugSymbol = "debug-symbol",
}
type TraceSpecPattern = string;

export interface Handler {
    id: HandlerId;
    flavor: TargetFlavor;
    scope: ScopeId;
    display_name: string;
    address: string | null;
    config: HandlerConfig;
}
export type HandlerId = number;
export type TargetFlavor = "insn" | "c" | "objc" | "swift" | "java";
export type ScopeId = string;

interface HandlerConfig {
    muted: boolean;
    capture_backtraces: boolean;
}

export type TabId = "events" | "disassembly" | "memory";

export type StagedItem = [id: StagedItemId, scope: ScopeName, member: MemberName];
export type StagedItemId = number;

export type ScopeName = string;
export type MemberName = string | [string, string];

export type Event = [
    targetId: HandlerId,
    timestamp: number,
    threadId: number,
    depth: number,
    caller: string | null,
    backtrace: string[] | null,
    message: string,
    style: string[]
];

export interface ProcessDetails {
    id: number;
    platform: Platform;
    arch: Architecture;
    pointer_size: number;
    page_size: number;
    main_module: NativeModule;
}

export interface NativeModule {
    base: string;
    name: string;
    path: string;
    size: number;
}

export type DisassemblyTarget = FunctionTarget | InstructionTarget;

export interface FunctionTarget {
    type: "function";
    name?: string;
    address: bigint;
}

export interface InstructionTarget {
    type: "instruction";
    address: bigint;
}

interface RpcState {
    pendingRequests: Map<RequestId, ResponseCallbacks<any>>;
    nextRequestId: number;
}

type RequestType = keyof RequestPayload;
type RequestId = number;
interface RequestPayload {
    "tracer:respawn": {};
    "handler:load": {
        id: HandlerId;
    };
    "handler:save": {
        id: HandlerId;
        code: string;
    };
    "handler:configure": {
        id: HandlerId;
        parameters: Record<string, boolean | number | string>;
    };
    "targets:stage": {
        profile: {
            spec: TraceSpec;
        };
    };
    "targets:commit": {
        id: StagedItemId | null;
    };
    "memory:read": {
        address: string;
        size: number;
    };
    "symbols:resolve-addresses": {
        addresses: string[];
    };
}
interface ResponsePayload {
    "tracer:respawn": void;
    "handler:load": {
        code: string;
        config: HandlerConfig;
    };
    "handler:save": void;
    "handler:configure": void;
    "targets:stage": {
        items: StagedItem[];
    };
    "targets:commit": {
        ids: HandlerId[];
        errors: TraceError[];
    };
    "memory:read": number[] | null;
    "symbols:resolve-addresses": {
        names: string[];
    };
}

interface TraceError {
    id: HandlerId;
    name: string;
    message: string;
}

interface ResponseCallbacks<T extends RequestType> {
    resolve(payload: ResponsePayload[T]): void;
    reject(error: Error): void;
}

type TracerMessage =
    | TracerSyncMessage
    | HandlersAddMessage
    | EventsAddMessage
    | RequestResultMessage
    | RequestErrorMessage
    ;

interface TracerSyncMessage {
    type: "tracer:sync";
    spawned_program: string | null;
    process: ProcessDetails;
    handlers: Handler[];
}

interface HandlersAddMessage {
    type: "handlers:add";
    handlers: Handler[];
}

interface EventsAddMessage {
    type: "events:add";
    events: Event[];
}

interface RequestResultMessage<T extends RequestType = any> {
    type: "request:result";
    id: RequestId;
    payload: ResponsePayload[T];
}

interface RequestErrorMessage {
    type: "request:error";
    id: RequestId;
    payload: {
        message: string;
        stack: string;
    };
}

interface AppNavigationState {
    handler: HandlerId | null;
    tab: TabId;
    eventIndex: number | null;
    disassemblyTarget: DisassemblyTarget | undefined;
    memoryLocation: bigint | undefined;
}

function inferDevModeHost(): string {
    const tokens = window.location.host.split(":");
    const host = tokens[0];
    const port = parseInt(tokens[1]) - 1;
    return `ws://${host}:${port}`;
}
