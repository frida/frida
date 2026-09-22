from __future__ import annotations

from typing import Mapping

from .model import (Customizations, EnumerationCustomizations,
                    EnumerationMemberCustomizations, MethodCustomizations,
                    ObjectTypeCustomizations, SignalCustomizations,
                    TypeCustomizations)

_DEVICE_ICON = """\
    public var icon: Icon? {
        guard let variant = frida_device_get_icon(handle) else {
            return nil
        }
        let dict = Marshal.valueFromVariant(variant) as! [String: Any]
        return Marshal.iconFromVarDict(dict)
    }"""

_PARAMETERS_ICONS = """\
    public var icons: [Icon] {
        guard let iconDicts = parameters["icons"] as? [[String: Any]] else {
            return []
        }
        return iconDicts.map(Marshal.iconFromVarDict)
    }"""

_PROCESS_MEMBERS = ("    public var id: UInt {\n"
                    "        return pid\n"
                    "    }\n\n") + _PARAMETERS_ICONS


def _json_message_method(*param_typings: str) -> MethodCustomizations:
    return MethodCustomizations(
        param_typings=list(param_typings),
        custom_logic="let json = Marshal.jsonFromValue(message)",
    )


def _sync_method(*param_typings: str) -> MethodCustomizations:
    return MethodCustomizations(param_typings=list(param_typings))


def load_customizations() -> Customizations:
    type_customizations: Mapping[str, TypeCustomizations] = {
        # Gadget-internal enumerations whose C values are non-sequential
        # (syscall numbers etc.) and which are not part of the Swift surface.
        # FridaRuntime (glib/other) is unreferenced in the surface and its
        # name would clash with the Runtime scheduling helper asset.
        "Runtime": EnumerationCustomizations(drop=True),
        "XnuMachTrap": EnumerationCustomizations(drop=True),
        "XnuBsdSyscall": EnumerationCustomizations(drop=True),
        "GadgetBreakpointAction": EnumerationCustomizations(drop=True),
        # The gir "value" attribute for this enum is unreliable (duplicated),
        # but the C enum genuinely starts at 1 and runs sequentially.
        "SessionDetachReason": EnumerationCustomizations(first_value=1),
        # Internal RPC plumbing exposed via higher-level facades (Script.rpc);
        # RpcClient.call takes a Json.Node which is not part of this surface.
        "RpcClient": ObjectTypeCustomizations(drop=True),
        "RpcPeer": ObjectTypeCustomizations(drop=True),
        "Injector": ObjectTypeCustomizations(drop=True),
        # The C member is "default"; the pre-cutout Swift API spelled it "auto".
        "ScriptRuntime": EnumerationCustomizations(
            members={"default": EnumerationMemberCustomizations(swift_name="auto")},
        ),
        # Generated classes with signal-driven AsyncStream events.
        "Device": ObjectTypeCustomizations(
            generate_signals=True,
            conformances=["Identifiable"],
            extra_members=_DEVICE_ICON,
            methods={
                "is_lost": MethodCustomizations(as_property=True),
                "get_dtype": MethodCustomizations(property_name="type"),
            },
            signals={
                "output": SignalCustomizations(
                    payload=[("data", "data"), ("fd", "fd"), ("pid", "pid")],
                ),
                "lost": SignalCustomizations(terminal=True),
            },
        ),
        "Session": ObjectTypeCustomizations(
            generate_signals=True,
            methods={
                "is_detached": MethodCustomizations(as_property=True),
                "create_script_from_bytes": MethodCustomizations(swift_name="createScript"),
            },
            signals={
                "detached": SignalCustomizations(
                    terminal=True,
                    payload=[("reason", "reason"), ("crash", "crash")],
                ),
            },
        ),
        # env is a dict (converted to KEY=VALUE strv), matching the Python
        # binding; envp stays the raw [String]?.
        "SpawnOptions": ObjectTypeCustomizations(
            methods={
                "set_env": MethodCustomizations(
                    swift_type="[String: String]?",
                    strv_converter="envpFromDictionary",
                ),
            },
        ),
        "Application": ObjectTypeCustomizations(extra_members=_PARAMETERS_ICONS),
        "Child": ObjectTypeCustomizations(
            methods={
                "get_envp": MethodCustomizations(
                    swift_type="[String: String]?",
                    strv_converter="dictionaryFromEnvp",
                ),
            },
        ),
        "Service": ObjectTypeCustomizations(
            methods={"is_closed": MethodCustomizations(as_property=True)},
            # The message signal marshals a GLib.Variant; deferred to a facade.
            signals={"message": SignalCustomizations(drop=True)},
        ),
        # Generated classes whose bespoke members (events, message/RPC handlers,
        # custom constructors) are injected from asset snippets.
        "Bus": ObjectTypeCustomizations(
            sendable=True,
            generate_signals=True,
            methods={
                "is_detached": MethodCustomizations(as_property=True),
                "post": _json_message_method("_ message: Any", "data: [UInt8]? = nil"),
            },
            signals={
                "detached": SignalCustomizations(terminal=True),
                "message": SignalCustomizations(
                    payload=[("message", "json"), ("data", "data")],
                    transform={0: ("Any", "Marshal.valueFromJSON")},
                ),
            },
        ),
        "Script": ObjectTypeCustomizations(
            sendable=True,
            methods={
                "is_destroyed": MethodCustomizations(as_property=True),
                "post": _json_message_method("_ message: Any", "data: [UInt8]? = nil"),
            },
            custom_members="script_members.swift",
            custom_init=(
                "        _exports = Exports()\n"
                "        _exports.script = self\n"
                '        connectSignal(instance: self, handle: handle, '
                'signal: "destroyed", handler: onDestroyed)\n'
                '        connectSignal(instance: self, handle: handle, '
                'signal: "message", handler: onMessage)'
            ),
            custom_deinit="        eventSource.finish()",
        ),
        "DeviceManager": ObjectTypeCustomizations(
            sendable=True,
            custom_members="device_manager_members.swift",
            custom_module="device_manager_module.swift",
            custom_init=(
                '        connectSignal(instance: self, handle: handle, '
                'signal: "added", handler: onAdded)\n'
                '        connectSignal(instance: self, handle: handle, '
                'signal: "removed", handler: onRemoved)\n'
                "        Task {\n"
                "            await self.performInitialDiscovery()\n"
                "        }"
            ),
            methods={
                "add_remote_device": MethodCustomizations(
                    custom_body="device_manager_add_remote_device.swift"),
                "add_barebone_device": MethodCustomizations(
                    custom_body="device_manager_add_barebone_device.swift"),
                **{
                    name: MethodCustomizations(drop=True)
                    for name in [
                        "enumerate_devices",
                        "get_device_by_id", "get_device_by_type",
                        "find_device_by_id", "find_device_by_type",
                    ]
                },
            },
        ),
        "PortalService": ObjectTypeCustomizations(
            sendable=True,
            custom_members="portal_service_members.swift",
            custom_init="\n".join(
                f'        connectSignal(instance: self, handle: handle, '
                f'signal: "{sig}", handler: on{h})'
                for sig, h in [
                    ("authenticated", "Authenticated"),
                    ("controller-connected", "ControllerConnected"),
                    ("controller-disconnected", "ControllerDisconnected"),
                    ("message", "Message"),
                    ("node-connected", "NodeConnected"),
                    ("node-disconnected", "NodeDisconnected"),
                    ("node-joined", "NodeJoined"),
                    ("node-left", "NodeLeft"),
                    ("subscribe", "Subscribe"),
                ]
            ),
            custom_deinit="        eventSource.finish()",
            methods={
                "get_cluster_params": MethodCustomizations(property_name="clusterParameters"),
                "get_control_params": MethodCustomizations(property_name="controlParameters"),
                "post": _json_message_method(
                    "to connectionId: ConnectionID", "message: Any", "data: [UInt8]? = nil"),
                "narrowcast": _json_message_method(
                    "tag: String", "message: Any", "data: [UInt8]? = nil"),
                "broadcast": _json_message_method("message: Any", "data: [UInt8]? = nil"),
                "kick": _sync_method("_ connectionId: ConnectionID"),
                "tag": _sync_method("_ connectionId: ConnectionID", "tag: String"),
                "untag": _sync_method("_ connectionId: ConnectionID", "tag: String"),
                "enumerate_tags": _sync_method("for connectionId: ConnectionID"),
                "add_cluster_endpoint": _sync_method("_ endpointParams: EndpointParameters"),
                "add_control_endpoint": _sync_method("_ endpointParams: EndpointParameters"),
            },
        ),
        "Compiler": ObjectTypeCustomizations(
            sendable=True,
            custom_members="compiler_members.swift",
            custom_init=(
                '        connectSignal(instance: self, handle: handle, '
                'signal: "starting", handler: onStarting)\n'
                '        connectSignal(instance: self, handle: handle, '
                'signal: "finished", handler: onFinished)\n'
                '        connectSignal(instance: self, handle: handle, '
                'signal: "output", handler: onOutput)\n'
                '        connectSignal(instance: self, handle: handle, '
                'signal: "diagnostics", handler: onDiagnostics)'
            ),
            custom_deinit="        eventSource.finish()",
        ),
        "PackageManager": ObjectTypeCustomizations(
            sendable=True,
            custom_members="package_manager_members.swift",
            custom_init=(
                '        connectSignal(instance: self, handle: handle, '
                'signal: "install-progress", handler: onInstallProgress)'
            ),
            custom_deinit="        eventSource.finish()",
            methods={
                "get_registry": MethodCustomizations(drop=True),
            },
        ),
        "Package": ObjectTypeCustomizations(
            sendable=True,
            custom_members="package_members.swift",
            methods={
                "get_description": MethodCustomizations(property_name="descriptionText"),
            },
        ),
        "PackageSearchResult": ObjectTypeCustomizations(
            sendable=True,
            custom_members="package_search_result_members.swift",
        ),
        "PackageInstallResult": ObjectTypeCustomizations(
            sendable=True,
            custom_members="package_install_result_members.swift",
        ),
        "FileMonitor": ObjectTypeCustomizations(
            generate_signals=True,
            custom_members="file_monitor_members.swift",
        ),
        "LanguageServer": ObjectTypeCustomizations(
            sendable=True,
            generate_signals=True,
            methods={
                "post": _sync_method("_ json: String"),
            },
        ),
        "EndpointParameters": ObjectTypeCustomizations(
            sendable=True,
            custom_members="endpoint_parameters_members.swift",
        ),
        # WebRequest wraps a borrowed handle (ref on init); WebResponse owns
        # the handle produced by its constructor.
        "WebRequest": ObjectTypeCustomizations(
            sendable=True,
            custom_members="web_request_members.swift",
            custom_init="        g_object_ref(gpointer(handle))",
        ),
        "WebResponse": ObjectTypeCustomizations(
            sendable=True,
            custom_members="web_response_members.swift",
            methods={"add_header": _sync_method("_ name: String", "_ val: String")},
        ),
        # Restore the pre-cutover conformances the hand-written classes carried.
        "Process": ObjectTypeCustomizations(
            conformances=["Identifiable"],
            extra_members=_PROCESS_MEMBERS,
        ),
        "PortalMembership": ObjectTypeCustomizations(
            conformances=["@unchecked Sendable"],
        ),
        "ControlService": ObjectTypeCustomizations(drop=True),
        "StaticAuthenticationService": ObjectTypeCustomizations(drop=True),
    }

    return Customizations(type_customizations=type_customizations)
