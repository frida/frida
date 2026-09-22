@frozen
public enum Icon {
    case rgba(
        width: Int,
        height: Int,
        pixels: [UInt8]
    )

    case png(
        data: [UInt8]
    )
}
