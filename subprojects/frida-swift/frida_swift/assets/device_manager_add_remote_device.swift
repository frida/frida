    @discardableResult
    public func addRemoteDevice(
        address: String,
        certificate: String? = nil,
        origin: String? = nil,
        token: String? = nil,
        keepaliveInterval: Int? = nil
    ) async throws -> Device {
        let options = frida_remote_device_options_new()
        defer { g_object_unref(gpointer(options)) }

        if let certificate {
            let rawCertificate = try Marshal.certificateFromString(certificate)
            frida_remote_device_options_set_certificate(options, rawCertificate)
            g_object_unref(rawCertificate)
        }

        if let origin {
            frida_remote_device_options_set_origin(options, origin)
        }

        if let token {
            frida_remote_device_options_set_token(options, token)
        }

        if let keepaliveInterval {
            frida_remote_device_options_set_keepalive_interval(options, gint(keepaliveInterval))
        }

        g_object_ref(gpointer(options))

        let deviceHandle = try await fridaAsync(OpaquePointer.self) { op in
            frida_device_manager_add_remote_device(self.handle, address, options, op.cancellable, { sourcePtr, asyncResultPtr, userData in
                let op = InternalOp<OpaquePointer>.takeRetained(from: userData!)

                var rawError: UnsafeMutablePointer<GError>? = nil
                let rawDeviceHandle = frida_device_manager_add_remote_device_finish(OpaquePointer(sourcePtr), asyncResultPtr, &rawError)

                if let rawError {
                    op.resumeFailure(Marshal.takeNativeError(rawError))
                    return
                }

                op.resumeSuccess(rawDeviceHandle!)
            }, op.userData)

            g_object_unref(gpointer(options))
        }

        return await store.deviceForHandle(deviceHandle)
    }
