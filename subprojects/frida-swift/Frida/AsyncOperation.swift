internal import FridaCore

func fridaAsync<Result>(
    _ resultType: Result.Type,
    startOnFridaThread: @escaping (_ op: InternalOp<Result>) -> Void
) async throws -> Result {
    let cancelBox = CancelBox()

    return try await withTaskCancellationHandler {
        try await withCheckedThrowingContinuation { continuation in
            let op = InternalOp<Result>(
                succeed: { value in
                    continuation.resume(returning: value)
                },
                fail: { error in
                    continuation.resume(throwing: error)
                }
            )

            cancelBox.thunk = { [op] in
                op.cancelFromSwiftTask()
            }

            Runtime.scheduleOnFridaThread {
                startOnFridaThread(op)
            }
        }
    } onCancel: {
        cancelBox.thunk?()
    }
}

final class InternalOp<Result>: @unchecked Sendable {
    let cancellable: UnsafeMutablePointer<GCancellable>

    var payload: UnsafeMutableRawPointer?

    private let succeed: (Result) -> Void
    private let fail: (Swift.Error) -> Void

    init(
        succeed: @escaping (Result) -> Void,
        fail: @escaping (Swift.Error) -> Void
    ) {
        self.cancellable = g_cancellable_new()!
        self.succeed = succeed
        self.fail = fail
    }

    deinit {
        g_object_unref(gpointer(cancellable))
    }

    var userData: UnsafeMutableRawPointer {
        Unmanaged.passRetained(self).toOpaque()
    }

    static func takeRetained(from userData: UnsafeMutableRawPointer) -> InternalOp<Result> {
        Unmanaged<InternalOp<Result>>
            .fromOpaque(userData)
            .takeRetainedValue()
    }

    func resumeSuccess(_ value: Result) {
        self.succeed(value)
    }

    func resumeFailure(_ error: Swift.Error) {
        self.fail(error)
    }

    func cancelFromSwiftTask() {
        g_cancellable_cancel(cancellable)
    }
}

final class CancelBox: @unchecked Sendable {
    var thunk: (@Sendable () -> Void)?
    init() {}
}
