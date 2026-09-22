[CCode (cheader_filename = "CoreFoundation/CoreFoundation.h", cprefix = "CF", gir_namespace = "CoreFoundation", gir_version = "1.0")]
namespace CoreFoundation {
	[Compact]
	[CCode (cname = "struct __CFArray")]
	public class Array : Type {
		[CCode (cname = "CFArrayCreate")]
		public Array (Allocator? allocator, [CCode (array_length_type = "CFIndex")] Type[] values,
			ArrayCallBacks callbacks = ArrayCallBacks.DEFAULT);

		public Index length {
			[CCode (cname = "CFArrayGetCount")]
			get;
		}

		[CCode (cname = "CFArrayGetValueAtIndex")]
		public void * @get (Index i);
	}

	public struct ArrayCallBacks {
		[CCode (cname = "kCFTypeArrayCallBacks")]
		public static ArrayCallBacks DEFAULT;

		public Index version;
		public ArrayRetainCallBack retain;
		public ArrayReleaseCallBack release;
		public ArrayCopyDescriptionCallBack copyDescription;
		public ArrayEqualCallBack equal;
	}

	public delegate void * ArrayRetainCallBack (Allocator allocator, void * value);
	public delegate void ArrayReleaseCallBack (Allocator allocator, void * value);
	public delegate String ArrayCopyDescriptionCallBack (void * value);
	public delegate bool ArrayEqualCallBack (void * value1, void * value2);

	[Compact]
	[CCode (cname = "struct __CFArray")]
	public class MutableArray : Array {
		[CCode (cname = "CFArrayRemoveAllValues")]
		public void clear ();

		[CCode (cname = "CFArrayAppendValue")]
		public void add (void * value);
	}

	[Compact]
	[CCode (cname = "struct __CFDictionary")]
	public class Dictionary : Type {
		[CCode (cname = "CFDictionaryGetCount")]
		public Index count ();

		[CCode (cname = "CFDictionaryGetValue")]
		public void * @get (void * key);

		public string get_string_value (string key) {
			return ((String) @get (String.make (key))).to_string ();
		}
	}

	[Compact]
	[CCode (cname = "struct __CFDictionary")]
	public class MutableDictionary : Dictionary {
		[CCode (cname = "CFDictionaryRemoveAllValues")]
		public void clear ();

		[CCode (cname = "CFDictionaryAddValue")]
		public void add (void * key, void * value);

		[CCode (cname = "CFDictionaryRemoveValue")]
		public void remove (void * key);
	}

	[Compact]
	[CCode (cname = "const struct __CFString")]
	public class String : Type {
		public Index length {
			get { return _length (); }
		}

		public static String make (string str) {
			return from_cstring (null, str, StringEncoding.UTF8);
		}

		public string to_string () {
			var max_length = max_size_for_encoding (length, StringEncoding.UTF8) + 1;
			var buffer = new char[max_length];
			to_cstring (buffer, max_length, StringEncoding.UTF8);
			return (string) buffer;
		}

		[CCode (cname = "CFStringCreateWithCString")]
		private static String from_cstring (Allocator? allocator, uint8 * c_str, StringEncoding encoding);

		[CCode (cname = "CFStringGetCString")]
		private bool to_cstring (uint8 * buffer, Index buffer_size, StringEncoding encoding);

		[CCode (cname = "CFStringGetLength")]
		private Index _length ();

		[CCode (cname = "CFStringGetMaximumSizeForEncoding")]
		private static Index max_size_for_encoding (Index length, StringEncoding encoding);
	}

	[CCode (cname = "CFStringEncoding", cprefix = "kCFStringEncoding", has_type_id = false)]
	public enum StringEncoding {
		MacRoman,
		WindowsLatin1,
		ISOLatin1,
		NextStepLatin,
		ASCII,
		Unicode,
		UTF8,
		NonLossyASCII,
		UTF16,
		UTF16BE,
		UTF16LE,
		UTF32,
		UTF32BE,
		UTF32LE,
	}

	[Compact]
	[CCode (cname = "const void", ref_function = "CFRetain", unref_function = "CFRelease")]
	public class Type {
		[CCode (cname = "CFShow")]
		public void show ();

		public string to_string () {
			return description ().to_string ();
		}

		[CCode (cname = "CFCopyDescription")]
		private String description ();
	}

	[Compact]
	[CCode (cname = "CFAllocatorRef")]
	public class Allocator {
		[CCode (cname = "CFAllocatorGetDefault")]
		public static Allocator get_default ();
	}

	[CCode (cname = "CFIndex", has_type_id = false)]
	public struct Index : long {
	}
}
