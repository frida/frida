@dynamicCallable
public struct RpcFunction {
    unowned let script: Script
    let functionName: String

    init(script: Script, functionName: String) {
        self.script = script
        self.functionName = functionName
    }

    @discardableResult
    public func dynamicallyCall(withArguments args: [Any]) async throws -> Any {
        return try await script.rpcCall(functionName: functionName, args: args)
    }

    @discardableResult
    public func dynamicallyCall(withArguments args: [JSValue]) async throws -> Any {
        return try await script.rpcCall(functionName: functionName, args: args.map { $0.raw })
    }
}
