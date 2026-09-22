internal import FridaCore

extension GType {
    static let boolean: GType = makeFundamental(5)
    static let int64: GType = makeFundamental(10)
    static let double: GType = makeFundamental(15)
    static let string: GType = makeFundamental(16)

    private static let fundamentalShift: GType = 2

    @inline(__always)
    static func makeFundamental(_ x: GType) -> GType {
        return x << fundamentalShift
    }
}
