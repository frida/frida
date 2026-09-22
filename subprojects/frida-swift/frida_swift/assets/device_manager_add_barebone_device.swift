    @discardableResult
    public func addBareboneDevice(
        config: BareboneConfig,
        id: String? = nil,
        name: String? = nil,
        icon: Icon? = nil
    ) async throws -> Device {
        let options = frida_barebone_device_options_new()
        defer { g_object_unref(gpointer(options)) }

        if let id {
            frida_barebone_device_options_set_id(options, id)
        }

        if let name {
            frida_barebone_device_options_set_name(options, name)
        }

        if let icon {
            let rawIcon = Marshal.variantFromIcon(icon)
            frida_barebone_device_options_set_icon(options, rawIcon)
            g_variant_unref(rawIcon)
        }

        g_object_ref(gpointer(options))

        let deviceHandle = try await fridaAsync(OpaquePointer.self) { op in
            frida_device_manager_add_barebone_device(self.handle, config.handle, options, op.cancellable, { sourcePtr, asyncResultPtr, userData in
                let op = InternalOp<OpaquePointer>.takeRetained(from: userData!)

                var rawError: UnsafeMutablePointer<GError>? = nil
                let rawDeviceHandle = frida_device_manager_add_barebone_device_finish(OpaquePointer(sourcePtr), asyncResultPtr, &rawError)

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
