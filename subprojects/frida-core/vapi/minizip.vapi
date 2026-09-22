[CCode (cprefix = "", gir_namespace = "Minizip", gir_version = "1.0", lower_case_cprefix = "mz_")]
namespace Minizip {
	[SimpleType]
	[CCode (cheader_filename = "minizip/mz_strm.h,minizip/mz_zip.h,minizip/mz_zip_rw.h", cname = "gpointer", cprefix = "mz_zip_reader_",
		has_destroy_function = false)]
	public struct Reader {
		public static Reader create (out Reader stream = null);
		[CCode (cname = "mz_zip_reader_delete")]
		public static void destroy (ref Reader stream);

		public Status open_file (string path);
		public Status close ();

		public Status locate_entry (string filename, bool ignore_case);

		public Status entry_save_file (string path);
		public Status entry_save_buffer (uint8[] buf);
		public int32 entry_save_buffer_length ();
	}

	[CCode (cheader_filename = "minizip/mz.h", cname = "int32_t", cprefix = "MZ_", has_type_id = false)]
	public enum Status {
		OK,
		STREAM_ERROR,
		DATA_ERROR,
		MEM_ERROR,
		BUF_ERROR,
		VERSION_ERROR,

		END_OF_LIST,
		END_OF_STREAM,

		PARAM_ERROR,
		FORMAT_ERROR,
		INTERNAL_ERROR,
		CRC_ERROR,
		CRYPT_ERROR,
		EXIST_ERROR,
		PASSWORD_ERROR,
		SUPPORT_ERROR,
		HASH_ERROR,
		OPEN_ERROR,
		CLOSE_ERROR,
		SEEK_ERROR,
		TELL_ERROR,
		READ_ERROR,
		WRITE_ERROR,
		SIGN_ERROR,
		SYMLINK_ERROR,
	}
}
