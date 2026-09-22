#if canImport(AppKit)
import AppKit

public extension Icon {
    var nsImage: NSImage {
        let cg = self.cgImage
        return NSImage(
            cgImage: cg,
            size: NSSize(width: cg.width, height: cg.height)
        )
    }
}
#endif
