#if canImport(CoreGraphics)
import CoreGraphics
import Foundation
import ImageIO

public extension Icon {
    var cgImage: CGImage {
        switch self {
        case let .rgba(width, height, pixels):
            return Self.makeCGImageFromRGBA(
                width: width,
                height: height,
                pixels: pixels
            )
        case let .png(data):
            return Self.makeCGImageFromPNG(data)
        }
    }

    private static func makeCGImageFromPNG(_ bytes: [UInt8]) -> CGImage {
        let cfData = Data(bytes) as CFData
        guard let src = CGImageSourceCreateWithData(cfData, nil),
              let image = CGImageSourceCreateImageAtIndex(src, 0, nil)
        else {
            preconditionFailure("Failed to decode PNG icon from Frida")
        }
        return image
    }

    private static func makeCGImageFromRGBA(
        width: Int,
        height: Int,
        pixels: [UInt8]
    ) -> CGImage {
        let bitsPerComponent = 8
        let bytesPerPixel = 4
        let bytesPerRow = width * bytesPerPixel

        let cfData = Data(pixels) as CFData
        guard let provider = CGDataProvider(data: cfData) else {
            preconditionFailure("Failed to create CGDataProvider for RGBA icon")
        }

        let colorSpace = CGColorSpaceCreateDeviceRGB()

        let bitmapInfo = CGBitmapInfo.byteOrder32Big.union(
            CGBitmapInfo(
                rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
            )
        )

        guard let image = CGImage(
            width: width,
            height: height,
            bitsPerComponent: bitsPerComponent,
            bitsPerPixel: bytesPerPixel * bitsPerComponent,
            bytesPerRow: bytesPerRow,
            space: colorSpace,
            bitmapInfo: bitmapInfo,
            provider: provider,
            decode: nil,
            shouldInterpolate: false,
            intent: .defaultIntent
        ) else {
            preconditionFailure("Failed to build CGImage for RGBA icon")
        }

        return image
    }
}
#endif
