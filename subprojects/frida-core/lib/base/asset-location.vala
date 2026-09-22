namespace Frida {
	public class AssetLocation : Object {
		public string libdir {
			get;
			construct;
		}

		public static AssetLocation detect () {
			string? module_path = null;
			Gum.Address our_address = Gum.Address.from_pointer (Gum.strip_code_pointer ((void *) detect));

			Gum.Process.enumerate_modules ((details) => {
				var range = details.range;
				if (our_address >= range.base_address && our_address < range.base_address + range.size) {
					module_path = details.path;
					return false;
				}
				return true;
			});
			assert (module_path != null);

			string libdir = Path.get_dirname (module_path);
			string basename = Path.get_basename (libdir).down ();
			if (basename == "bin" || basename == "sbin") {
				string parent = Path.get_dirname (libdir);
				string candidate = Path.build_filename (parent, Config.FRIDA_LIBDIR);
				if (FileUtils.test (candidate, IS_DIR))
					libdir = candidate;
			}

			return new AssetLocation (libdir);
		}

		private AssetLocation (string libdir) {
			Object (libdir: libdir);
		}

		public string derive_asset_path (string arch, string filename) {
			string root = Path.build_filename (libdir, Config.FRIDA_LIBDIR_NAME);
			if (FileUtils.test (root, IS_DIR))
#if DARWIN
				return Path.build_filename (root, filename);
#else
				return Path.build_filename (root, arch, filename);
#endif
			return Path.build_filename (libdir, filename);
		}

		public string derive_plugin_path (string filename) {
			return Path.build_filename (libdir, Config.FRIDA_LIBDIR_NAME, "plugins", filename);
		}
	}
}
