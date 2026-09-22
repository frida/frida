namespace Frida {
	public sealed class CFArray<T> {
		private CoreFoundation.Array handle;

		public static CFArray<T> wrap<T> (void * handle) {
			return new CFArray<T> ((CoreFoundation.Array) handle);
		}

		private CFArray (CoreFoundation.Array handle) {
			this.handle = handle;
		}

		public Iterator<T> iterator () {
			return new Iterator<T> (handle);
		}

		public class Iterator<T> {
			public CoreFoundation.Array handle;
			private CoreFoundation.Index i = 0;
			private CoreFoundation.Index length;

			internal Iterator (CoreFoundation.Array arr) {
				handle = arr;
				length = arr.length;
			}

			public unowned T? next_value () {
				if (i == length)
					return null;
				return handle[i++];
			}
		}
	}
}
