[CCode (cheader_filename = "brotli/encode.h", gir_namespace = "BrotliEncoder", gir_version = "1.0")]
namespace Brotli {
	[Compact]
	[CCode (cname = "BrotliEncoderState", free_function = "BrotliEncoderDestroyInstance")]
	public class Encoder {
		[CCode (cname = "BrotliEncoderCreateInstance")]
		public Encoder (void * alloc_func = null, void * free_func = null, void * opaque = null);

		[CCode (cname = "BrotliEncoderSetParameter")]
		public bool set_parameter (Brotli.EncoderParameter param, uint32 val);

		[CCode (cname = "BrotliEncoderCompressStream")]
		public bool compress_stream (Brotli.EncoderOperation op, size_t * available_in, uint8 ** next_in,
			size_t * available_out = null, uint8 ** next_out = null, out size_t total_out = null);

		[CCode (cname = "BrotliEncoderIsFinished")]
		public bool is_finished ();
	}

	[CCode (cprefix = "BROTLI_MODE_", has_type_id = false)]
	public enum EncoderMode {
		GENERIC,
		TEXT,
		FONT,
	}

	[CCode (cprefix = "BROTLI_OPERATION_", has_type_id = false)]
	public enum EncoderOperation {
		PROCESS,
		FLUSH,
		FINISH,
		EMIT_METADATA,
	}

	[CCode (cprefix = "BROTLI_PARAM_", has_type_id = false)]
	public enum EncoderParameter {
		MODE,
		QUALITY,
		LGWIN,
		LGBLOCK,
		DISABLE_LITERAL_CONTEXT_MODELING,
		SIZE_HINT,
		LARGE_WINDOW,
		NPOSTFIX,
		NDIRECT,
		STREAM_OFFSET,
	}

	public const int MIN_WINDOW_BITS;
	public const int MAX_WINDOW_BITS;
	public const int LARGE_MAX_WINDOW_BITS;
	public const int MIN_INPUT_BLOCK_BITS;
	public const int MAX_INPUT_BLOCK_BITS;
	public const int MIN_QUALITY;
	public const int MAX_QUALITY;
}
