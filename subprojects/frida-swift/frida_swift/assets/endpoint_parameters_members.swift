    private var _requestHandler: WebRequestHandler?

    public convenience init(
        address: String? = nil,
        port: UInt16 = 0,
        certificate: GLib.TlsCertificate? = nil,
        origin: String? = nil,
        authService: AuthenticationService? = nil,
        assetRoot: GLib.File? = nil
    ) {
        Runtime.ensureInitialized()

        self.init(handle: frida_endpoint_parameters_new(
            address,
            port,
            UnsafeMutablePointer<GTlsCertificate>(certificate?.handle),
            origin,
            authService?.handle,
            assetRoot?.handle
        ))
    }

    public var assetRoot: GLib.File? {
        get {
            guard let raw = frida_endpoint_parameters_get_asset_root(handle) else { return nil }
            g_object_ref(gpointer(raw))
            return GLib.File(handle: raw)
        }
        set {
            frida_endpoint_parameters_set_asset_root(handle, newValue?.handle)
        }
    }

    public var requestHandler: WebRequestHandler? {
        get { _requestHandler }
        set {
            _requestHandler = newValue
            frida_endpoint_parameters_set_request_handler(handle, newValue?.handle)
        }
    }
