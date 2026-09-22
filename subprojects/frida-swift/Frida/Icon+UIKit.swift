#if canImport(UIKit)
import UIKit

public extension Icon {
    var uiImage: UIImage {
        UIImage(cgImage: self.cgImage)
    }
}
#endif
