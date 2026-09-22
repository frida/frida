namespace Frida.Test.Labrats {
	public static string path_to_executable (string name, Arch arch = Arch.CURRENT) {
		return path_to_file (name + os_arch_suffix (arch) + os_executable_suffix ());
	}

	public static string path_to_library (string name, Arch arch = Arch.CURRENT) {
		return path_to_file (name + os_arch_suffix (arch) + os_library_suffix ());
	}

	public static string path_to_file (string name) {
		return Path.build_filename (Path.get_dirname (Process.current.filename), "labrats", name);
	}
}
