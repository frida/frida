    public var packages: [Package] {
        Package.list(from: frida_package_search_result_get_packages(handle)!)
    }
