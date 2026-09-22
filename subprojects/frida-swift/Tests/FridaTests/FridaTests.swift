import FridaCore
import XCTest
@testable import Frida

// End-to-end tests exercising the generated bindings against the local device
// (an agent living inside frida-core). No external target is required; the
// local system session is reached via `attach(pid: 0)`, mirroring the Python
// binding's test suite.
final class FridaTests: XCTestCase {

    /// Returns the local device, keeping the manager alive for the caller.
    private func withLocalDevice<T>(
        _ body: (Device) async throws -> T
    ) async throws -> T {
        let manager = DeviceManager()
        defer { Task { try? await manager.close() } }
        for await devices in await manager.snapshots() {
            guard !devices.isEmpty else { continue }
            let device = devices.first(where: { $0.type == .local }) ?? devices[0]
            return try await body(device)
        }
        throw TestFailure.noLocalDevice
    }

    enum TestFailure: Swift.Error { case noLocalDevice; case unexpectedSuccess }

    // 1. Enumeration: the local device is present and has running processes.
    func testEnumerateDevicesAndProcesses() async throws {
        try await withLocalDevice { device in
            XCTAssertEqual(device.type, .local)
            XCTAssertFalse(device.id.isEmpty)

            let processes = try await device.enumerateProcesses()
            XCTAssertFalse(processes.isEmpty, "Expected running processes")
            XCTAssertTrue(processes.contains { $0.pid == UInt(ProcessInfo.processInfo.processIdentifier) })
        }
    }

    // 2. Enums/errors: a bad lookup maps to a typed Frida.Error case.
    func testErrorMapping() async throws {
        try await withLocalDevice { device in
            do {
                _ = try await device.getProcessByPid(pid: 0x7fff_fffe)
                throw TestFailure.unexpectedSuccess
            } catch let error as Frida.Error {
                switch error {
                case .processNotFound, .invalidArgument, .permissionDenied:
                    break  // an expected typed domain error
                default:
                    XCTFail("Unexpected Frida.Error: \(error)")
                }
            }
        }
    }

    // 3. attach(0) + createScript + load round-trip against the local session.
    func testAttachAndLoadScript() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)
            XCTAssertFalse(session.isDetached)

            let script = try await session.createScript(source: "const x = 1 + 1;")
            try await script.load()
            XCTAssertFalse(script.isDestroyed)

            try await script.unload()
            try await session.detach()
        }
    }

    // 4. Signals: a script `send()` is delivered through script.events.
    func testScriptMessageSignal() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)
            let script = try await session.createScript(source: "send({ hello: 'world' });")

            let received = expectation(description: "message delivered")
            let events = script.events
            let listener = Task {
                for await event in events {
                    if case let .message(message, _) = event {
                        if let dict = message as? [String: Any],
                           let payload = dict["payload"] as? [String: Any],
                           payload["hello"] as? String == "world" {
                            received.fulfill()
                            break
                        }
                    }
                }
            }

            try await script.load()
            await fulfillment(of: [received], timeout: 5.0)
            listener.cancel()

            try await script.unload()
            try await session.detach()
        }
    }

    // 5. RPC: rpc.exports.add(3, 4) == 7 over the local session.
    func testRpcExports() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)
            let script = try await session.createScript(source: """
                rpc.exports = {
                    add(a, b) { return a + b; }
                };
                """)
            try await script.load()

            let result = try await script.exports.add(3, 4)
            let value = (result as? NSNumber)?.intValue
            XCTAssertEqual(value, 7)

            try await script.unload()
            try await session.detach()
        }
    }

    // 6. Rich description: a detail record summarizes its fields.
    func testProcessDetailsDescription() async throws {
        try await withLocalDevice { device in
            let ownPid = UInt(ProcessInfo.processInfo.processIdentifier)
            let processes = try await device.enumerateProcesses()
            let own = try XCTUnwrap(processes.first { $0.pid == ownPid })
            let text = own.description
            XCTAssertTrue(text.contains("pid: \(ownPid)"), text)
            XCTAssertTrue(text.contains("name: \"\(own.name)\""), text)
        }
    }

    // 7. Gio.IOStream return: openChannel to an unreachable address throws.
    func testOpenChannelThrows() async throws {
        try await withLocalDevice { device in
            do {
                _ = try await device.openChannel(address: "tcp:127.0.0.1:1")
                throw TestFailure.unexpectedSuccess
            } catch is Frida.Error {
            }
        }
    }

    // 8. Terminal/replay signal: detaching delivers Session's terminal event.
    func testSessionDetachTerminalEvent() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)

            let detached = expectation(description: "session detached event")
            let events = session.events
            let listener = Task {
                for await event in events {
                    if case .detached = event {
                        detached.fulfill()
                        break
                    }
                }
            }

            try await session.detach()
            await fulfillment(of: [detached], timeout: 5.0)
            listener.cancel()
        }
    }

    // 8b. Pre-cutover public symbols preserved through the cutover.
    func testRestoredPublicSymbols() async throws {
        XCTAssertEqual(RelayKind.turnUdp.rawValue, 0)
        XCTAssertEqual(ScriptRuntime.auto.rawValue, 0)

        try await withLocalDevice { device in
            func requireIdentifiable<T: Identifiable>(_ value: T) {}
            requireIdentifiable(device)

            let ownPid = UInt(ProcessInfo.processInfo.processIdentifier)
            let processes = try await device.enumerateProcesses()
            let own = try XCTUnwrap(processes.first { $0.pid == ownPid })
            requireIdentifiable(own)
            XCTAssertEqual(own.id, own.pid)
        }
    }

    // 9. Any -> GVariant -> Any round-trips through the Marshal asset.
    func testVariantRoundTrip() throws {
        let input: [String: Any] = [
            "name": "frida",
            "count": Int64(7),
            "flag": true,
            "ratio": 1.5,
            "tags": ["a", "b"],
        ]
        let variant = g_variant_ref_sink(Marshal.variantFromValue(input))!
        defer { g_variant_unref(variant) }

        let output = try XCTUnwrap(Marshal.valueFromVariant(variant) as? [String: Any])
        XCTAssertEqual(output["name"] as? String, "frida")
        XCTAssertEqual(output["count"] as? Int64, 7)
        XCTAssertEqual(output["flag"] as? Bool, true)
        XCTAssertEqual(output["ratio"] as? Double, 1.5)
        XCTAssertEqual(output["tags"] as? [String], ["a", "b"])
    }

    // 10. Error propagation: loading an invalid script surfaces a Frida.Error.
    func testInvalidScriptThrows() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)
            do {
                let script = try await session.createScript(source: "@@@ not valid javascript @@@")
                try await script.load()
                throw TestFailure.unexpectedSuccess
            } catch is Frida.Error {
            }
            try await session.detach()
        }
    }

    // 11. Options + list marshalling: enumerateApplications does not throw.
    func testEnumerateApplications() async throws {
        try await withLocalDevice { device in
            let apps = try await device.enumerateApplications()
            for app in apps {
                XCTAssertFalse(app.identifier.isEmpty)
            }
        }
    }

    // 12. Service open path: requesting an unreachable service throws.
    func testOpenServiceThrows() async throws {
        try await withLocalDevice { device in
            do {
                _ = try await device.openService(address: "dbus:example.invalid")
                throw TestFailure.unexpectedSuccess
            } catch is Frida.Error {
            }
        }
    }

    // 13. Spawn lifecycle: spawn suspended (with an env dict), resume, kill.
    func testSpawnResumeKill() async throws {
        try await withLocalDevice { device in
            let pid = try await device.spawn(
                program: "/bin/sleep",
                argv: ["/bin/sleep", "30"],
                env: ["LUMA_TEST": "1"]
            )
            XCTAssertGreaterThan(pid, 0)
            try await device.resume(pid: pid)
            try await device.kill(pid: pid)
        }
    }

    // 14. Icon accessors are exposed and safe to read even when absent.
    func testIconAccessors() async throws {
        try await withLocalDevice { device in
            let _: Icon? = device.icon

            let processes = try await device.enumerateProcesses(scope: .full)
            let own = try XCTUnwrap(processes.first { $0.pid == UInt(ProcessInfo.processInfo.processIdentifier) })
            let icons: [Icon] = own.icons
            XCTAssertGreaterThanOrEqual(icons.count, 0)
        }
    }

    // 15. createScript(bytes:) overload round-trips a compiled script.
    func testCreateScriptFromBytes() async throws {
        try await withLocalDevice { device in
            let session = try await device.attach(pid: 0)
            let bytes = try await session.compileScript(source: "const x = 1 + 1;")
            let script = try await session.createScript(bytes: bytes)
            try await script.load()
            XCTAssertFalse(script.isDestroyed)
            try await script.unload()
            try await session.detach()
        }
    }

    // 16. Newly-generated shell classes construct via their custom inits and
    // expose their generated getters / events.
    func testGeneratedServiceClasses() throws {
        let manager = PackageManager()
        manager.registry = "https://registry.example/"
        XCTAssertEqual(manager.registry, "https://registry.example/")

        let monitor = FileMonitor(path: "/tmp")
        _ = monitor.events
        XCTAssertEqual(monitor.path, "/tmp")
    }

    // 16b. Options expand into labeled args (spawn-style), built inline into
    // the Frida*Options GObject; a bad build maps to a typed error.
    func testCompilerBuildOptionsExpansion() async throws {
        let compiler = Compiler()
        do {
            _ = try await compiler.build(
                entrypoint: "/does/not/exist.ts",
                sourceMaps: .included,
                compression: .terser
            )
            throw TestFailure.unexpectedSuccess
        } catch is Frida.Error {
        }
    }

    // 17. PortalService constructs from EndpointParameters (generated shell +
    // custom init) and exposes its bespoke members.
    func testPortalServiceConstruction() throws {
        let params = EndpointParameters(address: "127.0.0.1", port: 0)
        let service = PortalService(clusterParameters: params)
        XCTAssertFalse(service.description.isEmpty)
        _ = service.events
    }

    // 18. Generated interface implementation: a Swift subclass of the generated
    // AuthenticationService base receives the portal's auth dispatch through
    // the generated GObject/vtable thunks. Mirrors frida-python's
    // test_implemented_interface_receives_dispatch.
    func testImplementedInterfaceReceivesDispatch() async throws {
        final class Recorder: @unchecked Sendable { var tokens: [String] = [] }
        let recorder = Recorder()

        final class TestAuth: AuthenticationService {
            let recorder: Recorder
            init(_ recorder: Recorder) { self.recorder = recorder; super.init() }
            override func authenticate(token: String) async throws -> String {
                recorder.tokens.append(token)
                if token != "secret" {
                    throw Frida.Error.invalidArgument("wrong token")
                }
                return "{}"
            }
        }

        let controlPort = freePort()
        let control = EndpointParameters(
            address: "127.0.0.1", port: controlPort, authService: TestAuth(recorder))
        let cluster = EndpointParameters(address: "127.0.0.1", port: freePort())
        let service = PortalService(clusterParameters: cluster, controlParameters: control)
        try await service.start()

        do {
            do {
                let device = try await DeviceManager().addRemoteDevice(
                    address: "127.0.0.1:\(controlPort)", token: "wrong")
                _ = try await device.enumerateProcesses()
                XCTFail("expected the wrong token to be rejected")
            } catch is Frida.Error {
            }

            let device = try await DeviceManager().addRemoteDevice(
                address: "127.0.0.1:\(controlPort)", token: "secret")
            _ = try await device.enumerateProcesses()

            XCTAssertEqual(recorder.tokens, ["wrong", "secret"])
        }

        try await service.stop()
    }

    // 19. Generated WebRequestHandler implementation serves an HTTP response
    // through the generated interface thunks.
    func testWebRequestHandlerServesResponse() async throws {
        final class Handler: WebRequestHandler {
            override func handleRequest(request: WebRequest) async throws -> WebResponse? {
                WebResponse(status: 200, body: Array("pong".utf8))
            }
        }

        let controlPort = freePort()
        let control = EndpointParameters(address: "127.0.0.1", port: controlPort)
        control.requestHandler = Handler()
        let cluster = EndpointParameters(address: "127.0.0.1", port: freePort())
        let service = PortalService(clusterParameters: cluster, controlParameters: control)
        try await service.start()

        do {
            let url = URL(string: "http://127.0.0.1:\(controlPort)/ping")!
            let (data, _) = try await URLSession.shared.data(from: url)
            XCTAssertEqual(String(data: data, encoding: .utf8), "pong")
        }

        try await service.stop()
    }

    private func freePort() -> UInt16 {
        let fd = socket(AF_INET, SOCK_STREAM, 0)
        defer { close(fd) }
        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = 0
        addr.sin_addr.s_addr = inet_addr("127.0.0.1")
        _ = withUnsafeMutablePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.bind(fd, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        var bound = sockaddr_in()
        var len = socklen_t(MemoryLayout<sockaddr_in>.size)
        _ = withUnsafeMutablePointer(to: &bound) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                getsockname(fd, $0, &len)
            }
        }
        return UInt16(bigEndian: bound.sin_port)
    }
}
