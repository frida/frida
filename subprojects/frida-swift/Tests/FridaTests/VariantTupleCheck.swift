import XCTest
@testable import Frida

final class VariantTupleCheck: XCTestCase {
    func testTupleOfHandle() {
        for _ in 0..<10000 {
            let t = GLib.Variant(tuple: [.fileDescriptor(at: 0)])
            XCTAssertNotNil(t)
        }
    }

    func testTupleOfMixed() {
        for _ in 0..<10000 {
            let t = GLib.Variant(tuple: [GLib.Variant("hello"), GLib.Variant(UInt32(7))])
            XCTAssertNotNil(t)
        }
    }
}
