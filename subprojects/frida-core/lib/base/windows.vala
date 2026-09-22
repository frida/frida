namespace Frida {
	public sealed class StdioPipes : Object {
		public OutputStream input {
			get;
			construct;
		}

		public InputStream output {
			get;
			construct;
		}

		public InputStream error {
			get;
			construct;
		}

		public StdioPipes (OutputStream input, InputStream output, InputStream error) {
			Object (input: input, output: output, error: error);
		}
	}
}
