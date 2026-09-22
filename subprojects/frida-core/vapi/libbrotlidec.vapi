[CCode (cheader_filename = "brotli/decode.h", gir_namespace = "BrotliDecoder", gir_version = "1.0")]
namespace Brotli {
	[Compact]
	[CCode (cname = "BrotliDecoderState", free_function = "BrotliDecoderDestroyInstance")]
	public class Decoder {
		[CCode (cname = "BrotliDecoderCreateInstance")]
		public Decoder (void * alloc_func = null, void * free_func = null, void * opaque = null);

		[CCode (cname = "BrotliDecoderSetParameter")]
		public bool set_parameter (Brotli.DecoderParameter param, uint32 val);

		[CCode (cname = "BrotliDecoderDecompressStream")]
		public Brotli.DecoderResult decompress_stream (size_t * available_in, uint8 ** next_in,
			size_t * available_out = null, uint8 ** next_out = null, out size_t total_out = null);

		[CCode (cname = "BrotliDecoderIsFinished")]
		public bool is_finished ();

		[CCode (cname = "BrotliDecoderGetErrorCode")]
		public Brotli.DecoderErrorCode get_error_code ();
	}

	[CCode (cprefix = "BROTLI_DECODER_RESULT_", has_type_id = false)]
	public enum DecoderResult {
		ERROR,
		SUCCESS,
		NEEDS_MORE_INPUT,
		NEEDS_MORE_OUTPUT,
	}

	[CCode (cprefix = "BROTLI_DECODER_", has_type_id = false)]
	public enum DecoderErrorCode {
		NO_ERROR,
		SUCCESS,
		NEEDS_MORE_INPUT,
		NEEDS_MORE_OUTPUT,
		ERROR_FORMAT_EXUBERANT_NIBBLE,
		ERROR_FORMAT_RESERVED,
		ERROR_FORMAT_EXUBERANT_META_NIBBLE,
		ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET,
		ERROR_FORMAT_SIMPLE_HUFFMAN_SAME,
		ERROR_FORMAT_CL_SPACE,
		ERROR_FORMAT_HUFFMAN_SPACE,
		ERROR_FORMAT_CONTEXT_MAP_REPEAT,
		ERROR_FORMAT_BLOCK_LENGTH_1,
		ERROR_FORMAT_BLOCK_LENGTH_2,
		ERROR_FORMAT_TRANSFORM,
		ERROR_FORMAT_DICTIONARY,
		ERROR_FORMAT_WINDOW_BITS,
		ERROR_FORMAT_PADDING_1,
		ERROR_FORMAT_PADDING_2,
		ERROR_FORMAT_DISTANCE,
		ERROR_COMPOUND_DICTIONARY,
		ERROR_DICTIONARY_NOT_SET,
		ERROR_INVALID_ARGUMENTS,
		ERROR_ALLOC_CONTEXT_MODES,
		ERROR_ALLOC_TREE_GROUPS,
		ERROR_ALLOC_CONTEXT_MAP,
		ERROR_ALLOC_RING_BUFFER_1,
		ERROR_ALLOC_RING_BUFFER_2,
		ERROR_ALLOC_BLOCK_TYPE_TREES,
		ERROR_UNREACHABLE,
	}

	[CCode (cprefix = "BROTLI_DECODER_PARAM_", has_type_id = false)]
	public enum DecoderParameter {
		DISABLE_RING_BUFFER_REALLOCATION,
		LARGE_WINDOW,
	}
}
