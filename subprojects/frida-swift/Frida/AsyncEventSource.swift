import Foundation

public final class AsyncEventSource<Event>: @unchecked Sendable {
    public typealias Stream = AsyncStream<Event>

    private let lock = NSLock()
    private var nextID: Int = 0
    private var continuations: [Int: Stream.Continuation] = [:]
    private var isFinished = false
    private var terminalEvent: Event? = nil

    public init() {}

    public func makeStream() -> Stream {
        Stream { continuation in
            addSubscriber(continuation)
        }
    }

    public func yield(_ event: Event) {
        lock.lock()
        if isFinished {
            lock.unlock()
            return
        }
        let subscribers = Array(continuations.values)
        lock.unlock()

        for c in subscribers {
            c.yield(event)
        }
    }

    public func finish(replayLast last: Event? = nil) {
        lock.lock()
        if isFinished {
            lock.unlock()
            return
        }
        isFinished = true
        terminalEvent = last
        let subscribers = Array(continuations.values)
        continuations.removeAll()
        lock.unlock()

        for c in subscribers {
            if let last {
                c.yield(last)
            }
            c.finish()
        }
    }

    private func addSubscriber(_ continuation: Stream.Continuation) {
        lock.lock()
        if isFinished {
            let last = terminalEvent
            lock.unlock()
            if let last {
                continuation.yield(last)
            }
            continuation.finish()
            return
        }

        let id = nextID
        nextID &+= 1
        continuations[id] = continuation
        lock.unlock()

        continuation.onTermination = { [weak self] _ in
            self?.removeSubscriber(id)
        }
    }

    private func removeSubscriber(_ id: Int) {
        lock.lock()
        continuations.removeValue(forKey: id)
        lock.unlock()
    }
}
