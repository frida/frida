internal import FridaCore

extension GLib {
    public final class MainContext: @unchecked Sendable {
        public static let frida: MainContext = {
            Runtime.ensureInitialized()
            return MainContext(handle: frida_get_main_context() ?? g_main_context_ref_thread_default())
        }()

        private let handle: OpaquePointer

        private init(handle: OpaquePointer) {
            self.handle = handle
        }

        public func schedule(_ work: @escaping @Sendable () -> Void) {
            let source = g_idle_source_new()
            g_source_set_callback(
                source,
                { userData in
                    Unmanaged<ScheduledWork>.fromOpaque(userData!).takeRetainedValue().run()
                    return gboolean(0)
                },
                Unmanaged.passRetained(ScheduledWork(work)).toOpaque(),
                nil
            )
            g_source_attach(source, handle)
            g_source_unref(source)
        }

        public func perform<Value: Sendable>(_ work: @escaping @Sendable () throws -> Value) async throws -> Value {
            try await withCheckedThrowingContinuation { continuation in
                schedule {
                    continuation.resume(with: Result { try work() })
                }
            }
        }

        private final class ScheduledWork {
            private let work: @Sendable () -> Void

            init(_ work: @escaping @Sendable () -> Void) {
                self.work = work
            }

            func run() {
                work()
            }
        }
    }
}
