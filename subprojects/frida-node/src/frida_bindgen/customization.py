from __future__ import annotations

from pathlib import Path
from typing import List, Mapping

from .model import (ConstructorCustomizations, CustomCode, CustomDeclaration,
                    Customizations, CustomMethod, CustomType, CustomTypeKind,
                    EnumerationCustomizations, EnumerationMemberCustomizations,
                    KeepAliveCustomization, MethodCustomizations,
                    ObjectTypeCustomizations, PropertyCustomizations,
                    SignalCustomizations, TypeCustomizations)

ASSETS_DIR = Path(__file__).resolve().parent / "assets"
CUSTOMIZATION_FACADE_EXPORTS = (
    (ASSETS_DIR / "customization_facade.exports")
    .read_text(encoding="utf-8")
    .strip()
    .split("\n")
)
CUSTOMIZATION_FACADE_TS = (ASSETS_DIR / "customization_facade.ts").read_text(
    encoding="utf-8"
)
CUSTOMIZATION_HELPERS_IMPORTS = (
    (ASSETS_DIR / "customization_helpers.imports")
    .read_text(encoding="utf-8")
    .strip()
    .split("\n")
)
CUSTOMIZATION_HELPERS_TS = (ASSETS_DIR / "customization_helpers.ts").read_text(
    encoding="utf-8"
)


def load_customizations() -> Customizations:
    custom_types: List[CustomType] = {
        "TargetProcess": CustomType(CustomTypeKind.TYPE, "ProcessID | ProcessName"),
        "ProcessID": CustomType(CustomTypeKind.TYPE, "number"),
        "InjecteeID": CustomType(CustomTypeKind.TYPE, "number"),
        "FileDescriptor": CustomType(CustomTypeKind.TYPE, "number"),
        "ProcessName": CustomType(CustomTypeKind.TYPE, "string"),
        "SystemParameters": CustomType(
            CustomTypeKind.INTERFACE,
            """
/**
 * Operating System details.
 */
os: {
    /**
     * ID, e.g.: windows, macos, linux, ios, android, qnx, fedora, ubuntu, etc.
     */
    id: string;

    /**
     * Human-readable name, e.g. `"macOS"`.
     */
    name: string;

    /**
     * Human-readable version string, e.g. `"11.2.2"`.
     */
    version?: string;

    /**
     * Build version, e.g. `"21B91"`.
     */
    build?: string;
}

/**
 * Platform, same as `Process.platform` in GumJS.
 */
platform: "windows" | "darwin" | "linux" | "freebsd" | "qnx";

/**
 * Architecture, same as `Process.arch` in GumJS.
 */
arch: "ia32" | "x64" | "arm" | "arm64" | "mips";

/**
 * Hardware details.
 */
hardware?: {
    /**
     * Product type, e.g. `"iPad6,3"`.
     */
    product?: string;

    /**
     * Hardware platform, e.g. `"t8010"`.
     */
    platform?: string;

    /**
     * Hardware model, e.g. `"J71bAP"`.
     */
    model?: string;
}

/**
 * Level of access.
 */
access: "full" | "jailed";

/**
 * System name, e.g. `"Ole André’s iPhone"`.
 */
name?: string;

/**
 * iOS UDID (Unique Device ID).
 */
udid?: string;

/**
 * Details about cellular and networking interfaces.
 */
interfaces?: SystemInterface[];

/**
 * Android API level, e.g.: `30`.
 */
apiLevel?: number;

[name: string]: any;
            """,
        ),
        "SystemInterface": CustomType(
            CustomTypeKind.TYPE, "NetworkInterface | CellularInterface"
        ),
        "NetworkInterface": CustomType(
            CustomTypeKind.INTERFACE,
            """
type: "ethernet" | "wifi" | "bluetooth";

/**
 * MAC address, e.g.: `"aa:bb:cc:dd:ee:ff"`.
 */
address: string;
            """,
        ),
        "CellularInterface": CustomType(
            CustomTypeKind.INTERFACE,
            """
type: "cellular";

/**
 * Phone number, e.g. `"+47 123 45 678"`.
 */
phoneNumber: string;
            """,
        ),
        "SpawnOptions": CustomType(
            CustomTypeKind.INTERFACE,
            """
argv?: string[];
envp?: { [name: string]: string };
env?: { [name: string]: string };
cwd?: string;
stdio?: Stdio;

[name: string]: any;
            """,
        ),
        "RelayProperties": CustomType(
            CustomTypeKind.INTERFACE,
            """
address: string;
username: string;
password: string;
kind: RelayKind;
            """,
        ),
        "Message": CustomType(CustomTypeKind.TYPE, "SendMessage | ErrorMessage"),
        "MessageType": CustomType(
            CustomTypeKind.ENUM,
            """
Send = "send",
Error = "error"
            """,
        ),
        "SendMessage": CustomType(
            CustomTypeKind.INTERFACE,
            """
type: MessageType.Send;
payload: any;
            """,
        ),
        "ErrorMessage": CustomType(
            CustomTypeKind.INTERFACE,
            """
type: MessageType.Error;
description: string;
stack?: string;
fileName?: string;
lineNumber?: number;
columnNumber?: number;
            """,
        ),
        "ScriptLogHandler": CustomType(
            CustomTypeKind.TYPE, "(level: LogLevel, text: string) => void"
        ),
        "ScriptExports": CustomType(
            CustomTypeKind.INTERFACE,
            """
[name: string]: (...args: any[]) => Promise<any>;
            """,
        ),
        "LogLevel": CustomType(
            CustomTypeKind.ENUM,
            """
Info = "info",
Warning = "warning",
Error = "error",
            """,
        ),
        "EnableDebuggerOptions": CustomType(
            CustomTypeKind.INTERFACE,
            """
port?: number;
            """,
        ),
        "PortalServiceOptions": CustomType(
            CustomTypeKind.INTERFACE,
            """
clusterParams?: EndpointParameters;
controlParams?: EndpointParameters;
            """,
        ),
        "PortalConnectionId": CustomType(CustomTypeKind.TYPE, "number"),
        "PortalConnectionTag": CustomType(CustomTypeKind.TYPE, "string"),
        "EndpointParametersSubset": CustomType(
            CustomTypeKind.INTERFACE,
            """
address?: string;
port?: number;
certificate?: string;
origin?: string;
authentication?: AuthenticationScheme;
assetRoot?: string;
            """,
        ),
        "AuthenticationScheme": CustomType(
            CustomTypeKind.TYPE,
            "TokenAuthenticationScheme | CallbackAuthenticationScheme",
        ),
        "TokenAuthenticationScheme": CustomType(
            CustomTypeKind.INTERFACE,
            """
scheme: "token";
token: string;
            """,
        ),
        "CallbackAuthenticationScheme": CustomType(
            CustomTypeKind.INTERFACE,
            """
scheme: "callback";
callback: AuthenticationCallback;
            """,
        ),
        "AuthenticationCallback": CustomType(
            CustomTypeKind.TYPE,
            "(token: string) => Promise<AuthenticatedSessionInfo>",
        ),
        "AuthenticatedSessionInfo": CustomType(
            CustomTypeKind.INTERFACE,
            """
[key: string]: any;
            """,
        ),
        "SocketAddress": CustomType(
            CustomTypeKind.TYPE,
            "IPV4SocketAddress | IPV6SocketAddress | AnonymousUnixSocketAddress | PathUnixSocketAddress | AbstractUnixSocketAddress",
        ),
        "IPV4SocketAddress": CustomType(
            CustomTypeKind.INTERFACE,
            """
family: "ipv4";
address: string;
port: number;
            """,
        ),
        "IPV6SocketAddress": CustomType(
            CustomTypeKind.INTERFACE,
            """
family: "ipv6";
address: string;
port: number;
flowlabel: number;
scopeid: number;
            """,
        ),
        "AnonymousUnixSocketAddress": CustomType(
            CustomTypeKind.INTERFACE,
            """
family: "unix:anonymous";
            """,
        ),
        "PathUnixSocketAddress": CustomType(
            CustomTypeKind.INTERFACE,
            """
family: "unix:path";
path: string;
            """,
        ),
        "AbstractUnixSocketAddress": CustomType(
            CustomTypeKind.INTERFACE,
            """
family: "unix:abstract";
path: Buffer;
            """,
        ),
        "Variant": CustomType(
            CustomTypeKind.TYPE, "VariantValue | [type: symbol, value: VariantValue]"
        ),
        "VariantValue": CustomType(
            CustomTypeKind.TYPE,
            """
| boolean
| number
| string
| Buffer
| Variant[]
| VariantDict
            """,
        ),
        "VariantDict": CustomType(
            CustomTypeKind.INTERFACE,
            """
[key: string]: Variant;
            """,
        ),
    }

    type_customizations: Mapping[str, TypeCustomizations] = {
        "DeviceManager": ObjectTypeCustomizations(
            signals={
                "added": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "removed": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "changed": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
            },
            cleanup="close",
        ),
        "Device": ObjectTypeCustomizations(
            methods={
                "query_system_parameters": MethodCustomizations(
                    return_typing="Promise<SystemParameters>",
                    return_wrapper="as SystemParameters",
                ),
                "spawn": MethodCustomizations(
                    param_typings=[
                        "programOrArgv: string | string[]",
                        "opts?: SpawnOptions",
                        "cancellable?: Cancellable | null",
                    ],
                    return_typing="Promise<ProcessID>",
                    custom_logic="""
const options: RawSpawnOptions = {};

let program: string;
let argv;
if (typeof programOrArgv === "string") {
    program = programOrArgv;
    argv = opts?.argv;
} else {
    program = programOrArgv[0];
    argv = programOrArgv;
    if (argv.length === 1) {
        argv = undefined;
    }
}
if (argv !== undefined) {
    options.argv = argv;
}

if (opts !== undefined) {
    const envp = opts.envp;
    if (envp !== undefined) {
        options.envp = objectToStrv(envp);
    }

    const env = opts.env;
    if (env !== undefined) {
        options.env = objectToStrv(env);
    }

    const cwd = opts.cwd;
    if (cwd !== undefined) {
        options.cwd = cwd;
    }

    const stdio = opts.stdio;
    if (stdio !== undefined) {
        options.stdio = stdio;
    }

    options.aux = Object.fromEntries(Object.entries(opts).filter(([k, v]) => !STANDARD_SPAWN_OPTION_NAMES.has(k)));
}
                    """,
                ),
                "input": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "data: Buffer",
                        "cancellable?: Cancellable | null",
                    ],
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "resume": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "cancellable?: Cancellable | null",
                    ],
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "kill": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "cancellable?: Cancellable | null",
                    ],
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "attach": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "options?: SessionOptions",
                        "cancellable?: Cancellable | null",
                    ],
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "inject_library_file": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "path: string",
                        "entrypoint: string",
                        "data: string",
                        "cancellable?: Cancellable | null",
                    ],
                    return_typing="Promise<InjecteeID>",
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "inject_library_blob": MethodCustomizations(
                    param_typings=[
                        "target: TargetProcess",
                        "blob: Buffer",
                        "entrypoint: string",
                        "data: string",
                        "cancellable?: Cancellable | null",
                    ],
                    return_typing="Promise<InjecteeID>",
                    custom_logic="const pid = await this.#getPid(target, cancellable);",
                ),
                "open_channel": MethodCustomizations(
                    return_typing='Promise<import("stream").Duplex>',
                    return_wrapper="new IOStreamAdapter",
                ),
            },
            properties={
                "dtype": PropertyCustomizations(
                    js_name="type",
                ),
            },
            signals={
                "spawn-added": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "spawn-removed": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "child-added": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "child-removed": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "process-crashed": SignalCustomizations(
                    behavior="FDN_SIGNAL_KEEP_ALIVE"
                ),
                "output": SignalCustomizations(
                    behavior="FDN_SIGNAL_KEEP_ALIVE",
                    transform={
                        0: ("pid: ProcessID", None),
                        1: ("fd: FileDescriptor", None),
                    },
                ),
                "uninjected": SignalCustomizations(
                    behavior="FDN_SIGNAL_KEEP_ALIVE",
                    transform={
                        0: ("id: InjecteeID", None),
                    },
                ),
            },
            custom_code=CustomCode(
                methods=[
                    CustomMethod(
                        typing="getProcess(name: string, options?: ProcessMatchOptions, cancellable?: Cancellable | null): Promise<Process>",
                        code="""
async getProcess(name: string, options: ProcessMatchOptions = {}, cancellable?: Cancellable | null): Promise<Process> {
    const {
        scope = Scope.Minimal,
    } = options;
    const processes = await this.enumerateProcesses({ scope }, cancellable);
    const mm = new Minimatch(name.toLowerCase());
    const matching = processes.filter(process => mm.match(process.name.toLowerCase()));
    if (matching.length === 1) {
        return matching[0];
    } else if (matching.length > 1) {
        throw new Error("Ambiguous name; it matches: " + matching.map(process => `${process.name} (pid: ${process.pid})`).join(", "));
    } else {
        throw new Error("Process not found");
    }
}
""",
                    ),
                    CustomMethod(
                        typing=None,
                        code="""
async #getPid(target: TargetProcess, cancellable?: Cancellable | null): Promise<ProcessID> {
    if (typeof target === "number") {
        return target;
    }

    const process = await this.getProcess(target, {}, cancellable);
    return process.pid;
}
""",
                    ),
                ],
            ),
        ),
        "SpawnOptions": ObjectTypeCustomizations(js_name="RawSpawnOptions"),
        "Bus": ObjectTypeCustomizations(
            methods={
                "post": MethodCustomizations(
                    param_typings=[
                        "message: any",
                        "data?: Buffer | null",
                    ],
                    custom_logic="const json = JSON.stringify(message);",
                ),
            },
            signals={
                "detached": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
                "message": SignalCustomizations(
                    behavior="FDN_SIGNAL_KEEP_ALIVE",
                    transform={
                        0: ("message: any", "JSON.parse"),
                    },
                ),
            },
        ),
        "Service": ObjectTypeCustomizations(
            keep_alive=KeepAliveCustomization(
                is_destroyed_function="is_closed", destroy_signal_name="close"
            ),
        ),
        "Relay": ObjectTypeCustomizations(
            constructor=ConstructorCustomizations(
                param_typings=[
                    "properties: RelayProperties",
                ],
                custom_logic="const { address, username, password, kind } = properties;",
            ),
        ),
        "RelayKind": EnumerationCustomizations(
            members={
                "turn_udp": EnumerationMemberCustomizations(js_name="TurnUDP"),
                "turn_tcp": EnumerationMemberCustomizations(js_name="TurnTCP"),
                "turn_tls": EnumerationMemberCustomizations(js_name="TurnTLS"),
            },
        ),
        "Script": ObjectTypeCustomizations(
            methods={
                "is_destroyed": MethodCustomizations(hide=True),
                "post": MethodCustomizations(
                    param_typings=[
                        "message: any",
                        "data?: Buffer | null",
                    ],
                    custom_logic="const json = JSON.stringify(message);",
                ),
                "enable_debugger": MethodCustomizations(
                    param_typings=[
                        "options?: EnableDebuggerOptions",
                        "cancellable?: Cancellable | null",
                    ],
                    custom_logic="const port = options?.port ?? 0;",
                ),
            },
            signals={
                "message": SignalCustomizations(
                    transform={
                        0: ("message: Message", "JSON.parse"),
                    },
                    intercept="this.#services.handleMessageIntercept",
                ),
            },
            custom_code=CustomCode(
                declarations=[
                    CustomDeclaration(
                        typing=None, code="#services = new ScriptServices(this);"
                    ),
                    CustomDeclaration(
                        typing="logHandler: ScriptLogHandler",
                        code="logHandler: ScriptLogHandler = log;",
                    ),
                ],
                methods=[
                    CustomMethod(
                        typing="readonly isDestroyed: boolean",
                        code="""
get isDestroyed(): boolean {
    return this._isDestroyed();
}
""",
                    ),
                    CustomMethod(
                        typing="readonly exports: ScriptExports",
                        code="""
get exports(): ScriptExports {
    return this.#services.exportsProxy;
}
""",
                    ),
                    CustomMethod(
                        typing="readonly defaultLogHandler: ScriptLogHandler",
                        code="""
get defaultLogHandler(): ScriptLogHandler {
    return log;
}
""",
                    ),
                ],
            ),
            keep_alive=KeepAliveCustomization(
                is_destroyed_function="is_destroyed", destroy_signal_name="destroyed"
            ),
        ),
        "ScriptRuntime": EnumerationCustomizations(
            members={
                "qjs": EnumerationMemberCustomizations(js_name="QJS"),
            },
        ),
        "ControlService": ObjectTypeCustomizations(
            methods={
                "start": MethodCustomizations(ref_keep_alive=True),
                "stop": MethodCustomizations(unref_keep_alive=True),
                "get_endpoint_params": MethodCustomizations(drop=True),
            },
            properties={
                "endpoint-params": PropertyCustomizations(drop=True),
            },
        ),
        "PortalService": ObjectTypeCustomizations(
            constructor=ConstructorCustomizations(
                param_typings=[
                    "options?: PortalServiceOptions",
                ],
                custom_logic="""
const clusterParams = options?.clusterParams ?? new EndpointParameters();
const controlParams = options?.controlParams ?? null;
""",
            ),
            methods={
                "start": MethodCustomizations(ref_keep_alive=True),
                "stop": MethodCustomizations(unref_keep_alive=True),
                "kick": MethodCustomizations(
                    param_typings=[
                        "connectionId: PortalConnectionId",
                    ],
                ),
                "post": MethodCustomizations(
                    param_typings=[
                        "connectionId: PortalConnectionId",
                        "message: any",
                        "data?: Buffer | null",
                    ],
                    custom_logic="const json = JSON.stringify(message);",
                ),
                "narrowcast": MethodCustomizations(
                    param_typings=[
                        "tag: string",
                        "message: any",
                        "data?: Buffer | null",
                    ],
                    custom_logic="const json = JSON.stringify(message);",
                ),
                "broadcast": MethodCustomizations(
                    param_typings=[
                        "message: any",
                        "data?: Buffer | null",
                    ],
                    custom_logic="const json = JSON.stringify(message);",
                ),
                "enumerate_tags": MethodCustomizations(
                    param_typings=[
                        "connectionId: PortalConnectionId",
                    ],
                ),
                "tag": MethodCustomizations(
                    param_typings=[
                        "connectionId: PortalConnectionId",
                        "tag: string",
                    ],
                ),
                "untag": MethodCustomizations(
                    param_typings=[
                        "connectionId: PortalConnectionId",
                        "tag: string",
                    ],
                ),
            },
            signals={
                "node-connected": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("remoteAddress: SocketAddress", "parseSocketAddress"),
                    },
                ),
                "node-joined": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                    },
                ),
                "node-left": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                    },
                ),
                "node-disconnected": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("remoteAddress: SocketAddress", "parseSocketAddress"),
                    },
                ),
                "controller-connected": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("remoteAddress: SocketAddress", "parseSocketAddress"),
                    },
                ),
                "controller-disconnected": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("remoteAddress: SocketAddress", "parseSocketAddress"),
                    },
                ),
                "authenticated": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("sessionInfo: AuthenticatedSessionInfo", "JSON.parse"),
                    },
                ),
                "subscribe": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                    },
                ),
                "message": SignalCustomizations(
                    transform={
                        0: ("connectionId: PortalConnectionId", None),
                        1: ("message: any", "JSON.parse"),
                    },
                ),
            },
        ),
        "EndpointParameters": ObjectTypeCustomizations(
            constructor=ConstructorCustomizations(
                param_typings=[
                    "params?: EndpointParametersSubset",
                ],
                custom_logic="""
const address = params?.address ?? null;
const port = params?.port ?? 0;
const certificate = params?.certificate ?? null;
const origin = params?.origin ?? null;

let authService: AuthenticationService | null = null;
const auth = params?.authentication;
if (auth !== undefined) {
    if (auth.scheme === "token") {
        authService = new StaticAuthenticationService(auth.token);
    } else {
        authService = new CallbackAuthenticationService(auth.callback);
    }
}

const assetRoot = params?.assetRoot ?? null;
""",
            ),
        ),
        "Injector": ObjectTypeCustomizations(drop=True),
        "RpcClient": ObjectTypeCustomizations(drop=True),
        "RpcPeer": ObjectTypeCustomizations(drop=True),
        "Compiler": ObjectTypeCustomizations(
            signals={
                "output": SignalCustomizations(behavior="FDN_SIGNAL_KEEP_ALIVE"),
            },
        ),
        "Cancellable": ObjectTypeCustomizations(
            methods={
                "is_cancelled": MethodCustomizations(hide=True),
                "set_error_if_cancelled": MethodCustomizations(
                    js_name="throwIfCancelled",
                    return_typing="void",
                ),
                "make_pollfd": MethodCustomizations(drop=True),
                "release_fd": MethodCustomizations(drop=True),
                "source_new": MethodCustomizations(drop=True),
            },
            custom_code=CustomCode(
                methods=[
                    CustomMethod(
                        typing="readonly isCancelled: boolean",
                        code="""
get isCancelled(): boolean {
    return this._isCancelled();
}
""",
                    ),
                    CustomMethod(
                        typing="static withTimeout(ms: number): Cancellable",
                        code="""
static withTimeout(ms: number): Cancellable {
    const c = new Cancellable();
    setTimeout(() => c.cancel(), ms).unref();
    return c;
}
""",
                    ),
                    CustomMethod(
                        typing="combine(other: Cancellable): Cancellable",
                        code="""
combine(other: Cancellable): Cancellable {
    const c = new Cancellable();
    this.cancelled.connect(() => c.cancel());
    other.cancelled.connect(() => c.cancel());
    if (this.isCancelled)
        c.cancel();
    if (other.isCancelled)
        c.cancel();
    return c;
}
""",
                    ),
                ],
            ),
        ),
        "IOStream": ObjectTypeCustomizations(
            methods={
                "close": MethodCustomizations(drop=True),
                "close_async": MethodCustomizations(js_name="close"),
                "splice_async": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "InputStream": ObjectTypeCustomizations(
            methods={
                "close": MethodCustomizations(drop=True),
                "close_async": MethodCustomizations(js_name="close"),
                "read": MethodCustomizations(drop=True),
                "read_async": MethodCustomizations(drop=True),
                "read_all": MethodCustomizations(drop=True),
                "read_all_async": MethodCustomizations(drop=True),
                "read_bytes": MethodCustomizations(drop=True),
                "read_bytes_async": MethodCustomizations(js_name="read"),
                "skip": MethodCustomizations(drop=True),
                "skip_async": MethodCustomizations(js_name="skip"),
                "is_closed": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "OutputStream": ObjectTypeCustomizations(
            methods={
                "close": MethodCustomizations(drop=True),
                "close_async": MethodCustomizations(js_name="close"),
                "flush": MethodCustomizations(drop=True),
                "flush_async": MethodCustomizations(js_name="flush"),
                "write": MethodCustomizations(drop=True),
                "write_async": MethodCustomizations(drop=True),
                "write_all": MethodCustomizations(drop=True),
                "write_all_async": MethodCustomizations(drop=True),
                "write_bytes": MethodCustomizations(drop=True),
                "write_bytes_async": MethodCustomizations(js_name="write"),
                "writev": MethodCustomizations(drop=True),
                "writev_async": MethodCustomizations(drop=True),
                "writev_all": MethodCustomizations(drop=True),
                "writev_all_async": MethodCustomizations(drop=True),
                "splice": MethodCustomizations(drop=True),
                "splice_async": MethodCustomizations(drop=True),
                "is_closing": MethodCustomizations(drop=True),
                "is_closed": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "UnixSocketAddress": ObjectTypeCustomizations(
            methods={
                "get_path": MethodCustomizations(
                    return_typing="Buffer",
                    return_cconversion="fdn_buffer_to_value (env, (const guint8 *) retval, g_unix_socket_address_get_path_len (handle))",
                ),
                "get_path_len": MethodCustomizations(drop=True),
                "get_is_abstract": MethodCustomizations(drop=True),
            },
            properties={
                "path": PropertyCustomizations(typing="path: Buffer"),
                "abstract": PropertyCustomizations(drop=True),
                "path-as-array": PropertyCustomizations(drop=True),
            },
        ),
        "SocketAddress": ObjectTypeCustomizations(
            js_name="BaseSocketAddress",
            constructor=ConstructorCustomizations(drop=True),
            methods={
                "to_native": MethodCustomizations(drop=True),
            },
        ),
        "SocketConnectable": ObjectTypeCustomizations(drop_abstract_base=True),
        "InetAddress": ObjectTypeCustomizations(
            properties={
                "bytes": PropertyCustomizations(drop=True),
            },
        ),
        "Object": ObjectTypeCustomizations(
            js_name="BaseObject",
            methods={
                "is_floating": MethodCustomizations(drop=True),
                "ref": MethodCustomizations(drop=True),
                "ref_sink": MethodCustomizations(drop=True),
                "unref": MethodCustomizations(drop=True),
                "getv": MethodCustomizations(drop=True),
                "get_property": MethodCustomizations(drop=True),
                "set_property": MethodCustomizations(drop=True),
                "notify_by_pspec": MethodCustomizations(drop=True),
                "freeze_notify": MethodCustomizations(drop=True),
                "thaw_notify": MethodCustomizations(drop=True),
                "bind_property": MethodCustomizations(drop=True),
                "bind_property_full": MethodCustomizations(drop=True),
                "bind_property_with_closures": MethodCustomizations(drop=True),
                "force_floating": MethodCustomizations(drop=True),
                "get_data": MethodCustomizations(drop=True),
                "get_qdata": MethodCustomizations(drop=True),
                "run_dispose": MethodCustomizations(drop=True),
                "set_data": MethodCustomizations(drop=True),
                "steal_data": MethodCustomizations(drop=True),
                "steal_qdata": MethodCustomizations(drop=True),
                "watch_closure": MethodCustomizations(drop=True),
            },
            signals={
                "notify": SignalCustomizations(drop=True),
            },
        ),
    }

    return Customizations(
        custom_types,
        type_customizations,
        CUSTOMIZATION_FACADE_EXPORTS,
        CUSTOMIZATION_FACADE_TS,
        CUSTOMIZATION_HELPERS_IMPORTS,
        CUSTOMIZATION_HELPERS_TS,
    )
