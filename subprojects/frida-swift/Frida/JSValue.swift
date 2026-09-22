public struct JSValue: @unchecked Sendable {
    public let raw: Any

    public init(_ raw: Any) {
        self.raw = raw
    }
}
