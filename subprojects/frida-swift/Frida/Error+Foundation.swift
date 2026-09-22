#if canImport(Foundation)
import Foundation

extension Error: LocalizedError {
    public var errorDescription: String? {
        description
    }
}
#endif
