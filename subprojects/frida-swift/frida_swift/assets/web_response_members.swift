    public convenience init(status: UInt32, body: [UInt8]) {
        Runtime.ensureInitialized()
        let rawBody = Marshal.bytesFromArray(body)
        self.init(handle: frida_web_response_new(status, rawBody))
        g_bytes_unref(rawBody)
    }
