ProcessTarget = Union[int, str]


class ScriptErrorMessage(TypedDict):
    type: Literal["error"]
    description: str
    stack: NotRequired[str]
    fileName: NotRequired[str]
    lineNumber: NotRequired[int]
    columnNumber: NotRequired[int]


class ScriptPayloadMessage(TypedDict):
    type: Literal["send"]
    payload: NotRequired[Any]


ScriptMessage = Union[ScriptPayloadMessage, ScriptErrorMessage]
ScriptMessageCallback = Callable[[ScriptMessage, Optional[bytes]], None]
ScriptDestroyedCallback = Callable[[], None]

SessionDetachedCallback = Callable[
    [
        Literal[
            "application-requested", "process-replaced", "process-terminated", "connection-terminated", "device-lost"
        ],
        Optional[_frida.Crash],
    ],
    None,
]

BusDetachedCallback = Callable[[], None]
BusMessageCallback = Callable[[Mapping[Any, Any], Optional[bytes]], None]

ServiceCloseCallback = Callable[[], None]
ServiceMessageCallback = Callable[[Any], None]

DeviceSpawnAddedCallback = Callable[[_frida.Spawn], None]
DeviceSpawnRemovedCallback = Callable[[_frida.Spawn], None]
DeviceChildAddedCallback = Callable[[_frida.Child], None]
DeviceChildRemovedCallback = Callable[[_frida.Child], None]
DeviceProcessCrashedCallback = Callable[[_frida.Crash], None]
DeviceOutputCallback = Callable[[int, int, bytes], None]
DeviceUninjectedCallback = Callable[[int], None]
DeviceLostCallback = Callable[[], None]

DeviceManagerAddedCallback = Callable[[_frida.Device], None]
DeviceManagerRemovedCallback = Callable[[_frida.Device], None]
DeviceManagerChangedCallback = Callable[[], None]

PortalServiceNodeJoinedCallback = Callable[[int, _frida.Application], None]
PortalServiceNodeLeftCallback = Callable[[int, _frida.Application], None]
PortalServiceNodeConnectedCallback = Callable[[int, Tuple[str, int]], None]
PortalServiceNodeDisconnectedCallback = Callable[[int, Tuple[str, int]], None]
PortalServiceControllerConnectedCallback = Callable[[int, Tuple[str, int]], None]
PortalServiceControllerDisconnectedCallback = Callable[[int, Tuple[str, int]], None]
PortalServiceAuthenticatedCallback = Callable[[int, Mapping[Any, Any]], None]
PortalServiceSubscribeCallback = Callable[[int], None]
PortalServiceMessageCallback = Callable[[int, Mapping[Any, Any], Optional[bytes]], None]


class CompilerDiagnosticFile(TypedDict):
    path: str
    line: int
    character: int


class CompilerDiagnostic(TypedDict):
    category: str
    code: int
    file: NotRequired[CompilerDiagnosticFile]
    text: str


CompilerStartingCallback = Callable[[], None]
CompilerFinishedCallback = Callable[[], None]
CompilerOutputCallback = Callable[[str], None]
CompilerDiagnosticsCallback = Callable[[List[CompilerDiagnostic]], None]

CompilerOutputFormat = Literal["unescaped", "hex-bytes", "c-string"]
CompilerBundleFormat = Literal["esm", "iife"]
CompilerTypeCheck = Literal["full", "none"]
CompilerSourceMaps = Literal["included", "omitted"]
CompilerCompression = Literal["none", "terser"]
CompilerPlatform = Literal["neutral", "gum", "browser"]

PackageManagerInstallProgressCallback = Callable[
    [
        Literal[
            "initializing",
            "preparing-dependencies",
            "resolving-package",
            "fetching-resource",
            "package-already-installed",
            "downloading-package",
            "package-installed",
            "resolving-and-installing-all",
            "complete",
        ],
        float,
        Optional[str],
    ],
    None,
]

PackageRole = Literal["runtime", "development", "optional", "peer"]
