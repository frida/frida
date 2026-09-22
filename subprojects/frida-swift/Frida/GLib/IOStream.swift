internal import FridaCore

extension GLib {
    public final class IOStream: CustomStringConvertible, Equatable, Hashable {
        private let handle: UnsafeMutablePointer<GIOStream>
        private let input: UnsafeMutablePointer<GInputStream>
        private let output: UnsafeMutablePointer<GOutputStream>

        private let ioPriority: Int32 = 0

        init(handle: UnsafeMutablePointer<GIOStream>) {
            self.handle = handle
            self.input = g_io_stream_get_input_stream(handle)
            self.output = g_io_stream_get_output_stream(handle)
        }

        deinit {
            g_object_unref(gpointer(handle))
        }

        public var isClosed: Bool {
            g_io_stream_is_closed(handle) != 0
        }

        public var description: String {
            "GLib.IOStream()"
        }

        public static func == (lhs: IOStream, rhs: IOStream) -> Bool {
            lhs.handle == rhs.handle
        }

        public func hash(into hasher: inout Hasher) {
            hasher.combine(UInt(bitPattern: handle))
        }

        public func close(_ count: UInt) async throws {
            try await fridaAsync(Void.self) { op in
                let userData = op.userData

                g_io_stream_close_async(self.handle, self.ioPriority, op.cancellable, { sourcePtr, asyncResultPtr, userDataPtr in
                    let op = InternalOp<Void>.takeRetained(from: userDataPtr!)

                    var rawError: UnsafeMutablePointer<GError>? = nil
                    g_io_stream_close_finish(UnsafeMutablePointer<GIOStream>(OpaquePointer(sourcePtr)), asyncResultPtr, &rawError)

                    if let rawError {
                        op.resumeFailure(Marshal.takeNativeError(rawError))
                        return
                    }

                    op.resumeSuccess(())
                }, userData)
            }
        }

        public func read(_ count: UInt) async throws -> [UInt8] {
            try await fridaAsync([UInt8].self) { op in
                let userData = op.userData

                g_input_stream_read_bytes_async(self.input, numericCast(count), self.ioPriority, op.cancellable, { sourcePtr, asyncResultPtr, userDataPtr in
                    let op = InternalOp<[UInt8]>.takeRetained(from: userDataPtr!)

                    var rawError: UnsafeMutablePointer<GError>? = nil
                    let bytes = g_input_stream_read_bytes_finish(UnsafeMutablePointer<GInputStream>(OpaquePointer(sourcePtr)), asyncResultPtr,
                                                                 &rawError)

                    if let rawError {
                        op.resumeFailure(Marshal.takeNativeError(rawError))
                        return
                    }

                    let result = Marshal.arrayFromBytes(bytes)!
                    op.resumeSuccess(result)
                    g_bytes_unref(bytes)
                }, userData)
            }
        }

        public func readAll(_ count: UInt) async throws -> [UInt8] {
            try await fridaAsync([UInt8].self) { op in
                let buffer = g_malloc(numericCast(count))!
                op.payload = buffer
                let userData = op.userData

                g_input_stream_read_all_async(self.input, buffer, numericCast(count), self.ioPriority, op.cancellable,
                    { sourcePtr, asyncResultPtr, userDataPtr in
                        let op = InternalOp<[UInt8]>.takeRetained(from: userDataPtr!)

                        let buffer = op.payload!
                        op.payload = nil
                        defer { g_free(buffer) }

                        var bytesRead: gsize = 0
                        var rawError: UnsafeMutablePointer<GError>? = nil
                        g_input_stream_read_all_finish(UnsafeMutablePointer<GInputStream>(OpaquePointer(sourcePtr)), asyncResultPtr, &bytesRead,
                                                       &rawError)

                        if let rawError {
                            op.resumeFailure(Marshal.takeNativeError(rawError))
                            return
                        }

                        let length = Int(bytesRead)
                        var result = [UInt8](repeating: 0, count: length)
                        _ = result.withUnsafeMutableBytes { dstBuf in
                            memcpy(dstBuf.baseAddress!, buffer, length)
                        }

                        op.resumeSuccess(result)
                }, userData)
            }
        }

        public func write(_ data: [UInt8]) async throws -> UInt {
            try await fridaAsync(UInt.self) { op in
                let bytesHandle = Marshal.bytesFromArray(data)
                let userData = op.userData

                g_output_stream_write_bytes_async(self.output, bytesHandle, self.ioPriority, op.cancellable,
                    { sourcePtr, asyncResultPtr, userDataPtr in
                        let op = InternalOp<UInt>.takeRetained(from: userDataPtr!)

                        var rawError: UnsafeMutablePointer<GError>? = nil
                        let numBytesWritten = g_output_stream_write_bytes_finish(UnsafeMutablePointer<GOutputStream>(OpaquePointer(sourcePtr)),
                                                                                 asyncResultPtr, &rawError)

                        if let rawError {
                            op.resumeFailure(Marshal.takeNativeError(rawError))
                            return
                        }

                        op.resumeSuccess(UInt(numBytesWritten))
                }, userData)

                if let bytesHandle = bytesHandle {
                    g_bytes_unref(bytesHandle)
                }
            }
        }

        public func writeAll(_ data: [UInt8]) async throws {
            try await fridaAsync(Void.self) { op in
                let byteCount = data.count
                let buffer = g_malloc(gsize(byteCount))!
                _ = data.withUnsafeBytes { srcBuf in
                    memcpy(buffer, srcBuf.baseAddress!, byteCount)
                }

                op.payload = buffer
                let userData = op.userData

                g_output_stream_write_all_async(self.output, buffer, gsize(byteCount), self.ioPriority, op.cancellable,
                    { sourcePtr, asyncResultPtr, userDataPtr in
                        let op = InternalOp<Void>.takeRetained(from: userDataPtr!)

                        let buffer = op.payload!
                        op.payload = nil
                        defer { g_free(buffer) }

                        var bytesWritten: gsize = 0
                        var rawError: UnsafeMutablePointer<GError>? = nil
                        g_output_stream_write_all_finish(UnsafeMutablePointer<GOutputStream>(OpaquePointer(sourcePtr)), asyncResultPtr,
                                                         &bytesWritten, &rawError)

                        if let rawError {
                            op.resumeFailure(Marshal.takeNativeError(rawError))
                            return
                        }

                        op.resumeSuccess(())
                }, userData)
            }
        }
    }
}
