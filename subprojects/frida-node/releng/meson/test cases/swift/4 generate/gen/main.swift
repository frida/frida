#if os(OSX)
    import Darwin
#else
    import Glibc
#endif

#if swift(>=3.0)
let fname = CommandLine.arguments[1]
#else
let fname = Process.arguments[1]
#endif
let code = "public func getGenerated() -> Int {\n    return 42\n}\n"

let f = fopen(fname, "w")

fwrite(code, 1, Int(strlen(code)), f)
print("Name: \(fname)")
fclose(f)
