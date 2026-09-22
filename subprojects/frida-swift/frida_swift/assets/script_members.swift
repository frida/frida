    public var events: Events {
        eventSource.makeStream()
    }

    public typealias Events = AsyncStream<Event>

    @frozen
    public enum Event {
        case destroyed
        case message(message: Any, data: [UInt8]?)
    }

    private let eventSource = AsyncEventSource<Event>()

    private var _exports: Exports

    private var rpcContinuations: [Int: (RpcInternalResult) -> Void] = [:]
    private var requestId = 0

    public var exports: Exports {
        _exports
    }

    private let onDestroyed: @convention(c) (OpaquePointer, gpointer) -> Void = { _, userData in
        let connection = Unmanaged<SignalConnection<Script>>.fromOpaque(userData).takeUnretainedValue()

        if let script = connection.instance {
            script.failAllRpcContinuations(Error.rpcError(
                message: "Script destroyed",
                stackTrace: nil
            ))

            script.eventSource.finish(replayLast: .destroyed)
        }
    }

    private let onMessage: @convention(c) (OpaquePointer, UnsafePointer<gchar>, OpaquePointer?, gpointer) -> Void = { _, rawJson, rawData, userData in
        let connection = Unmanaged<SignalConnection<Script>>.fromOpaque(userData).takeUnretainedValue()

        guard let script = connection.instance else { return }

        let jsonString = String(cString: rawJson)
        guard let parsedAny = try? Marshal.valueFromJSON(jsonString) else { return }
        let dataBytes = Marshal.arrayFromBytes(rawData)

        guard
            let msgDict = parsedAny as? [String: Any],
            let payloadAny = msgDict["payload"],
            let payload = payloadAny as? [Any],
            payload.count >= 3,
            let kind = payload[0] as? String,
            kind == "frida:rpc",
            let requestId = payload[1] as? Int,
            let status = payload[2] as? String
        else {
            script.publish(.message(message: parsedAny, data: dataBytes))
            return
        }

        guard let cont = script.rpcContinuations[requestId] else {
            return
        }
        script.rpcContinuations[requestId] = nil

        if status == "ok" {
            if let db = dataBytes {
                if payload.count >= 5 {
                    let valuePart = payload[4]
                    cont(.success(value: [valuePart, db]))
                } else {
                    cont(.success(value: db))
                }
            } else {
                let valuePart: Any = (payload.count >= 4) ? payload[3] : JSONNull.null
                cont(.success(value: valuePart))
            }
        } else {
            let message = (payload.count >= 4 ? (payload[3] as? String ?? "") : "")
            let stackTrace = (payload.count >= 6 ? payload[5] as? String : nil)
            cont(.error(Error.rpcError(message: message, stackTrace: stackTrace)))
        }
    }

    private func publish(_ event: Event) {
        eventSource.yield(event)
    }

    @dynamicMemberLookup
    public struct Exports {
        unowned var script: Script!

        public subscript(dynamicMember functionName: String) -> RpcFunction {
            get {
                return RpcFunction(script: self.script, functionName: functionName)
            }
        }
    }

    internal func rpcCall(functionName: String, args: [Any]) async throws -> Any {
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Any, Swift.Error>) in
            Runtime.scheduleOnFridaThread { [weak self] in
                guard let self else {
                    cont.resume(throwing: Error.rpcError(
                        message: "Script deallocated before RPC could be scheduled",
                        stackTrace: nil
                    ))
                    return
                }

                let id = self.requestId
                self.requestId &+= 1

                let message: [Any] = [
                    "frida:rpc",
                    id,
                    "call",
                    functionName,
                    args
                ]

                self.rpcContinuations[id] = { result in
                    switch result {
                    case let .success(value):
                        cont.resume(returning: value)
                    case let .error(error):
                        cont.resume(throwing: error)
                    }
                }

                self.post(message)
            }
        }
    }

    private func failAllRpcContinuations(_ error: Swift.Error) {
        let pending = rpcContinuations.values
        rpcContinuations.removeAll()

        for cont in pending {
            cont(.error(error))
        }
    }

    func makeRequestId() -> Int {
        let currentId = requestId
        requestId += 1
        return currentId
    }

    internal enum RpcInternalResult {
        case success(value: Any)
        case error(Swift.Error)
    }
