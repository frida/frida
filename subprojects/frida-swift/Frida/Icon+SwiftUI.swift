#if canImport(SwiftUI)
import SwiftUI

public extension Icon {
    var swiftUIImage: Image {
        #if canImport(AppKit)
        return Image(nsImage: self.nsImage)
        #elseif canImport(UIKit)
        return Image(uiImage: self.uiImage)
        #else
        return Image(
            decorative: self.cgImage,
            scale: 1.0,
            orientation: .up
        )
        #endif
    }
}
#endif
