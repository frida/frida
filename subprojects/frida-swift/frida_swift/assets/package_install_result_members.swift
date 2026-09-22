    public var packages: [Package] {
        Package.list(from: frida_package_install_result_get_packages(handle)!)
    }
