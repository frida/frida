[CCode (lower_case_cprefix = "OPENSSL_", gir_namespace = "OpenSSL", gir_version = "1.0")]
namespace OpenSSL {
	[Compact]
	[CCode (cheader_filename = "openssl/ssl.h", cname = "SSL_CTX", cprefix = "SSL_CTX_")]
	public class SSLContext {
		public SSLContext (SSLMethod meth);

		[CCode (cname = "SSL_CTX_use_certificate")]
		public int use_certificate (X509 cert);
		[CCode (cname = "SSL_CTX_use_PrivateKey")]
		public int use_private_key (Envelope.Key key);
	}

	[Compact]
	[CCode (cheader_filename = "openssl/ssl.h", cname = "SSL", cprefix = "SSL_")]
	public class SSL {
		public SSL (SSLContext ctx);

		public void * get_app_data ();
		public void set_app_data (void * data);

		public void set_connect_state ();
		public void set_accept_state ();

		public int set_cipher_list (string str);
		public int set_alpn_protos (uint8[] protos);
		public void set_psk_client_callback (PskClientCallback callback);

		public void set_quic_transport_version (int version);

		public void set_bio (void * rbio, void * wbio);

		public int do_handshake ();
		public int read (uint8[] buf);
		public int write (uint8[] buf);
		public int shutdown ();
		public int get_error (int ret);
		public int pending ();
	}

	[CCode (cheader_filename = "openssl/ssl.h", cname = "int", cprefix = "SSL_ERROR_", has_type_id = false)]
	public enum SSLErrorCode {
		NONE,
		SSL,
		WANT_READ,
		WANT_WRITE,
		WANT_X509_LOOKUP,
		SYSCALL,
		ZERO_RETURN,
		WANT_CONNECT,
		WANT_ACCEPT,
	}

	[CCode (has_target = false)]
	public delegate uint PskClientCallback (SSL ssl, string? hint, char[] identity, uint8[] psk);

	[Compact]
	[CCode (cheader_filename = "openssl/ssl.h", cname = "SSL_METHOD", cprefix = "SSL_METHOD_", free_function = "")]
	public class SSLMethod {
		[CCode (cname = "TLS_method")]
		public static unowned SSLMethod tls ();
		[CCode (cname = "TLS_server_method")]
		public static unowned SSLMethod tls_server ();
		[CCode (cname = "TLS_client_method")]
		public static unowned SSLMethod tls_client ();

		[CCode (cname = "DTLS_method")]
		public static unowned SSLMethod dtls ();
		[CCode (cname = "DTLS_server_method")]
		public static unowned SSLMethod dtls_server ();
		[CCode (cname = "DTLS_client_method")]
		public static unowned SSLMethod dtls_client ();
	}

	[CCode (lower_case_cprefix = "TLSEXT_TYPE_")]
	namespace TLSExtensionType {
		public const int quic_transport_parameters;
	}

	[Compact]
	[CCode (cname = "X509", cprefix = "X509_")]
	public class X509 {
		public X509 ();

		[CCode (cname = "X509_get_serialNumber")]
		public unowned ASN1.Integer get_serial_number ();

		[CCode (cname = "X509_getm_notBefore")]
		public unowned ASN1.Time get_not_before ();
		[CCode (cname = "X509_getm_notAfter")]
		public unowned ASN1.Time get_not_after ();

		public unowned Name get_subject_name ();
		public int set_issuer_name (Name name);

		public int set_pubkey (Envelope.Key key);

		public int sign_ctx (Envelope.MessageDigestContext ctx);

		[CCode (cname = "X509_get_X509_PUBKEY")]
		public unowned X509PublicKey get_public_key ();

		[CCode (cname = "i2d_X509_bio", instance_pos = 2)]
		public int to_der (BasicIO sink);

		[CCode (cname = "PEM_write_bio_X509", instance_pos = 2)]
		public int to_pem (BasicIO sink);

		[Compact]
		[CCode (cname = "X509_NAME", cprefix = "X509_NAME_")]
		public class Name {
			public int add_entry_by_txt (string field, ASN1.MultiByteStringType type, uint8[] bytes, int loc = -1, int set = 0);
		}
	}

	[Compact]
	[CCode (cname = "X509_PUBKEY", cprefix = "X509_PUBKEY_", free_function = "")]
	public class X509PublicKey {
		[CCode (cname = "i2d_X509_PUBKEY_bio", instance_pos = 2)]
		public int to_der (BasicIO sink);

		[CCode (cname = "PEM_write_bio_X509_PUBKEY", instance_pos = 2)]
		public int to_pem (BasicIO sink);
	}

	[Compact]
	[CCode (cname = "BIO", cprefix = "BIO_")]
	public class BasicIO {
		public BasicIO (BasicIOMethod method);
		[CCode (cname = "BIO_new_mem_buf")]
		public BasicIO.from_static_memory_buffer (uint8[] buf);

		public long get_mem_data ([CCode (array_length = false)] out unowned uint8[] data);

		public int read (uint8[] buf);
		public int write (uint8[] buf);
		public size_t ctrl_pending ();

		[CCode (cname = "BIO_new")]
		public static void * raw_new (BasicIOMethod method);
		[CCode (cname = "BIO_free")]
		public static int raw_free (void * bio);
		[CCode (cname = "BIO_read")]
		public static int raw_read (void * bio, uint8[] buf);
		[CCode (cname = "BIO_write")]
		public static int raw_write (void * bio, uint8[] buf);
		[CCode (cname = "BIO_ctrl_pending")]
		public static size_t raw_ctrl_pending (void * bio);
		[CCode (cname = "BIO_get_mem_data")]
		public static long raw_get_mem_data (void * bio, [CCode (array_length = false)] out unowned uint8[] data);
	}

	[Compact]
	[CCode (cname = "BIO_METHOD", cprefix = "BIO_METHOD_", free_function = "")]
	public class BasicIOMethod {
		[CCode (cname = "BIO_s_mem")]
		public static unowned BasicIOMethod memory ();
	}

	[CCode (cheader_filename = "openssl/asn1.h")]
	namespace ASN1 {
		[Compact]
		[CCode (cname = "ASN1_INTEGER", cprefix = "ASN1_INTEGER_")]
		public class Integer {
			public int set_int64 (int64 v);
			public int set_uint64 (uint64 v);
		}

		[Compact]
		[CCode (cname = "ASN1_TIME", cprefix = "ASN1_TIME_")]
		public class Time {
			[CCode (cname = "X509_gmtime_adj")]
			public unowned Time adjust (long delta);
		}

		[CCode (cname = "int", cprefix = "MBSTRING_", has_type_id = false)]
		public enum MultiByteStringType {
			UTF8,
			[CCode (cname = "MBSTRING_ASC")]
			ASCII,
		}
	}

	[CCode (cheader_filename = "openssl/evp.h")]
	namespace Envelope {
		[Compact]
		[CCode (cname = "EVP_PKEY_CTX", cprefix = "EVP_PKEY_", free_function = "EVP_PKEY_CTX_free")]
		public class KeyContext {
			[CCode (cname = "EVP_PKEY_CTX_new")]
			public KeyContext.for_key (Key key, Engine? engine = null);
			[CCode (cname = "EVP_PKEY_CTX_new_id")]
			public KeyContext.for_key_type (KeyType type, Engine? engine = null);

			public int keygen_init ();
			public int keygen (ref Key pkey);

			public int derive_init ();
			public int derive_set_peer (Key peer);
			public int derive ([CCode (array_length = false)] uint8[]? key, ref size_t keylen);
		}

		[Compact]
		[CCode (cname = "EVP_PKEY", cprefix = "EVP_PKEY_", copy_function = "EVP_PKEY_dup")]
		public class Key {
			[CCode (cheader_filename = "openssl/x509.h", cname = "d2i_PUBKEY_bio")]
			public Key.from_der (BasicIO source, Key ** a = null);
			[CCode (cname = "EVP_PKEY_new_raw_public_key")]
			public Key.from_raw_public_key (KeyType type, Engine? engine, uint8[] pub);
			[CCode (cname = "EVP_PKEY_new_raw_private_key")]
			public Key.from_raw_private_key (KeyType type, Engine? engine, uint8[] priv);

			public int get_raw_public_key ([CCode (array_length = false)] uint8[]? pub, ref size_t len);
			public int get_raw_private_key ([CCode (array_length = false)] uint8[]? priv, ref size_t len);

			[CCode (cheader_filename = "openssl/x509.h", cname = "i2d_PUBKEY_bio", instance_pos = 2)]
			public int to_der (BasicIO sink);

			[CCode (cname = "PEM_write_bio_PUBKEY", instance_pos = 2)]
			public int to_pem (BasicIO sink);
		}

		[Compact]
		[CCode (cname = "EVP_PKEY_CTX", cprefix = "EVP_PKEY_CTX_")]
		public class PublicKeyContext {
		}

		[CCode (cname = "int", cprefix = "EVP_PKEY_", has_type_id = false)]
		public enum KeyType {
			NONE,
			RSA,
			RSA2,
			RSA_PSS,
			DSA,
			DSA1,
			DSA2,
			DSA3,
			DSA4,
			DH,
			DHX,
			EC,
			SM2,
			HMAC,
			CMAC,
			SCRYPT,
			TLS1_PRF,
			HKDF,
			POLY1305,
			SIPHASH,
			X25519,
			ED25519,
			X448,
			ED448,
			KEYMGMT,
		}

		[Compact]
		[CCode (cheader_filename = "openssl/kdf.h", cname = "EVP_KDF", cprefix = "EVP_KDF_")]
		public class KeyDerivationFunction {
			public static KeyDerivationFunction? fetch (LibraryContext? ctx, string algorithm, string? properties = null);
		}

		[Compact]
		[CCode (cheader_filename = "openssl/kdf.h", cname = "EVP_KDF_CTX", cprefix = "EVP_KDF_",
			free_function = "EVP_KDF_CTX_free")]
		public class KeyDerivationContext {
			[CCode (cname = "EVP_KDF_CTX_new")]
			public KeyDerivationContext (KeyDerivationFunction kdf);

			public int derive (uint8[] key, [CCode (array_length = false)] Param[] params);
		}

		[CCode (cheader_filename = "openssl/core_names.h", lower_case_cprefix = "OSSL_KDF_NAME_")]
		namespace KeyDerivationAlgorithm {
			public const string HKDF;
			public const string TLS1_3_KDF;
			public const string PBKDF1;
			public const string PBKDF2;
			public const string SCRYPT;
			public const string SSHKDF;
			public const string SSKDF;
			public const string TLS1_PRF;
			public const string X942KDF_ASN1;
			public const string X942KDF_CONCAT;
			public const string X963KDF;
			public const string KBKDF;
			public const string KRB5KDF;
		}

		[CCode (cheader_filename = "openssl/core_names.h", lower_case_cprefix = "OSSL_KDF_PARAM_")]
		namespace KeyDerivationParameter {
			public const string SECRET;
			public const string KEY;
			public const string SALT;
			public const string PASSWORD;
			public const string PREFIX;
			public const string LABEL;
			public const string DATA;
			public const string DIGEST;
			public const string CIPHER;
			public const string MAC;
			public const string MAC_SIZE;
			public const string PROPERTIES;
			public const string ITER;
			public const string MODE;
			public const string PKCS5;
			public const string UKM;
			public const string CEK_ALG;
			public const string SCRYPT_N;
			public const string SCRYPT_R;
			public const string SCRYPT_P;
			public const string SCRYPT_MAXMEM;
			public const string INFO;
			public const string SEED;
			public const string SSHKDF_XCGHASH;
			public const string SSHKDF_SESSION_ID;
			public const string SSHKDF_TYPE;
			public const string SIZE;
			public const string CONSTANT;
			public const string PKCS12_ID;
			public const string KBKDF_USE_L;
			public const string KBKDF_USE_SEPARATOR;
			public const string X942_ACVPINFO;
			public const string X942_PARTYUINFO;
			public const string X942_PARTYVINFO;
			public const string X942_SUPP_PUBINFO;
			public const string X942_SUPP_PRIVINFO;
			public const string X942_USE_KEYBITS;
		}

		[Compact]
		[CCode (cname = "EVP_CIPHER_CTX", cprefix = "EVP_CIPHER_CTX_")]
		public class CipherContext {
			public CipherContext ();

			public int reset ();

			public int ctrl (CipherCtrlType type, int arg, void * ptr);

			[CCode (cname = "EVP_EncryptInit")]
			public int encrypt_init (Cipher cipher,
				[CCode (array_length = false)] uint8[] key,
				[CCode (array_length = false)] uint8[] iv);
			[CCode (cname = "EVP_EncryptUpdate")]
			public int encrypt_update ([CCode (array_length = false)] uint8[] output, ref int outlen, uint8[] input);
			[CCode (cname = "EVP_EncryptFinal")]
			public int encrypt_final ([CCode (array_length = false)] uint8[] output, ref int outlen);

			[CCode (cname = "EVP_DecryptInit")]
			public int decrypt_init (Cipher cipher,
				[CCode (array_length = false)] uint8[] key,
				[CCode (array_length = false)] uint8[] iv);
			[CCode (cname = "EVP_DecryptUpdate")]
			public int decrypt_update ([CCode (array_length = false)] uint8[] output, ref int outlen, uint8[] input);
			[CCode (cname = "EVP_DecryptFinal")]
			public int decrypt_final ([CCode (array_length = false)] uint8[] output, ref int outlen);
		}

		[Compact]
		[CCode (cname = "EVP_CIPHER", cprefix = "EVP_CIPHER_")]
		public class Cipher {
			public static Cipher? fetch (LibraryContext? ctx, string algorithm, string? properties = null);
		}

		[CCode (cheader_filename = "openssl/evp.h", cname = "int", cprefix = "EVP_CTRL_", has_type_id = false)]
		public enum CipherCtrlType {
			INIT,
			SET_KEY_LENGTH,
			GET_RC2_KEY_BITS,
			SET_RC2_KEY_BITS,
			GET_RC5_ROUNDS,
			SET_RC5_ROUNDS,
			RAND_KEY,
			PBE_PRF_NID,
			COPY,
			AEAD_SET_IVLEN,
			AEAD_GET_TAG,
			AEAD_SET_TAG,
			AEAD_SET_IV_FIXED,
			GCM_SET_IVLEN,
			GCM_GET_TAG,
			GCM_SET_TAG,
			GCM_SET_IV_FIXED,
			GCM_IV_GEN,
			CCM_SET_IVLEN,
			CCM_GET_TAG,
			CCM_SET_TAG,
			CCM_SET_IV_FIXED,
			CCM_SET_L,
			CCM_SET_MSGLEN,
			AEAD_TLS1_AAD,
			AEAD_SET_MAC_KEY,
			GCM_SET_IV_INV,
			TLS1_1_MULTIBLOCK_AAD,
			TLS1_1_MULTIBLOCK_ENCRYPT,
			TLS1_1_MULTIBLOCK_DECRYPT,
			TLS1_1_MULTIBLOCK_MAX_BUFSIZE,
			SSL3_MASTER_SECRET,
			SET_SBOX,
			SBOX_USED,
			KEY_MESH,
			BLOCK_PADDING_MODE,
			SET_PIPELINE_OUTPUT_BUFS,
			SET_PIPELINE_INPUT_BUFS,
			SET_PIPELINE_INPUT_LENS,
			GET_IVLEN,
			SET_SPEED,
			PROCESS_UNPROTECTED,
			GET_WRAP_CIPHER,
			TLSTREE,
		}

		[Compact]
		[CCode (cname = "EVP_MD_CTX", cprefix = "EVP_MD_CTX_")]
		public class MessageDigestContext {
			public MessageDigestContext ();

			[CCode (cname = "EVP_DigestSignInit")]
			public int digest_sign_init (PublicKeyContext ** pctx, MessageDigest? type, Engine? engine = null, Key? key = null);
			[CCode (cname = "EVP_DigestSignUpdate")]
			public int digest_sign_update (uint8[] data);
			[CCode (cname = "EVP_DigestSignFinal")]
			public int digest_sign_final ([CCode (array_length = false)] uint8[]? sigret, ref size_t siglen);
			[CCode (cname = "EVP_DigestSign")]
			public int digest_sign ([CCode (array_length = false)] uint8[]? sigret, ref size_t siglen, uint8[] tbs);
		}

		[Compact]
		[CCode (cname = "EVP_MD", cprefix = "EVP_MD_")]
		public class MessageDigest {
			public static MessageDigest fetch (LibraryContext? ctx, string algorithm, string? properties = null);
		}

		[Compact]
		[CCode (cname = "EVP_MAC_CTX", cprefix = "EVP_MAC_CTX_")]
		public class MessageAuthCodeContext {
			public MessageAuthCodeContext (MessageAuthCode mac);

			[CCode (cname = "EVP_MAC_init")]
			public int init (uint8[] key, [CCode (array_length = false)] Param[]? params = null);
			[CCode (cname = "EVP_MAC_update")]
			public int update (uint8[] data);
			[CCode (cname = "EVP_MAC_final")]
			public int final ([CCode (array_length_pos = 2)] uint8[]? outdata, out size_t outlen);
		}

		[Compact]
		[CCode (cname = "EVP_MAC", cprefix = "EVP_MAC_")]
		public class MessageAuthCode {
			public static MessageAuthCode fetch (LibraryContext? ctx, string algorithm, string? properties = null);
		}

		[CCode (cheader_filename = "openssl/core_names.h", lower_case_cprefix = "OSSL_MAC_PARAM_")]
		namespace MessageAuthParameter {
			public const string KEY;
			public const string IV;
			public const string CUSTOM;
			public const string SALT;
			public const string XOF;
			public const string DIGEST_NOINIT;
			public const string DIGEST_ONESHOT;
			public const string C_ROUNDS;
			public const string D_ROUNDS;
			public const string CIPHER;
			public const string DIGEST;
			public const string PROPERTIES;
			public const string SIZE;
			public const string BLOCK_SIZE;
			public const string TLS_DATA_SIZE;
		}
	}

	[Compact]
	[CCode (cheader_filename = "openssl/engine.h", cname = "ENGINE")]
	public class Engine {
	}

	[Compact]
	[CCode (cheader_filename = "openssl/types.h", cname = "OSSL_LIB_CTX")]
	public class LibraryContext {
	}

	[CCode (cheader_filename = "openssl/core.h", cname = "OSSL_PARAM", has_copy_function = false, has_destroy_function = false)]
	public struct Param {
		public unowned string? key;
		public ParamDataType data_type;
		[CCode (array_length_cname = "data_size")]
		public unowned uint8[]? data;
		public size_t return_size;
	}

	[CCode (cheader_filename = "openssl/core.h", cname = "guint", cprefix = "OSSL_PARAM_", has_type_id = false)]
	public enum ParamDataType {
		INTEGER,
		UNSIGNED_INTEGER,
		REAL,
		UTF8_STRING,
		OCTET_STRING,
		UTF8_PTR,
		OCTET_PTR,
	}

	[CCode (cheader_filename = "openssl/core.h", cname = "size_t", cprefix = "OSSL_PARAM_", has_type_id = false)]
	public enum ParamReturnSize {
		UNMODIFIED,
	}

	[CCode (cheader_filename = "openssl/objects.h", lower_case_cprefix = "SN_")]
	namespace ShortName {
		public const string sha256;
		public const string sha384;
		public const string sha512;
		public const string siphash;
		public const string chacha20_poly1305;
	}

	[CCode (cheader_filename = "openssl/rand.h")]
	namespace Rng {
		[CCode (cname = "RAND_bytes")]
		public int generate (uint8[] buf);
	}

	[Compact]
	[CCode (cname = "BN_CTX", cprefix = "BN_CTX_")]
	public class BigNumberContext {
		[CCode (cname = "BN_CTX_secure_new")]
		public BigNumberContext.secure ();
	}

	[Compact]
	[CCode (cname = "BIGNUM", cprefix = "BN_")]
	public class BigNumber {
		public BigNumber ();
		[CCode (cname = "BN_native2bn")]
		public BigNumber.from_native (uint8[] data, BigNumber * ret = null);
		[CCode (cname = "BN_bin2bn")]
		public BigNumber.from_big_endian (uint8[] data, BigNumber * ret = null);

		public static BigNumber get_rfc2409_prime_768 (BigNumber * ret = null);
		public static BigNumber get_rfc2409_prime_1024 (BigNumber * ret = null);

		public static BigNumber get_rfc3526_prime_1536 (BigNumber * ret = null);
		public static BigNumber get_rfc3526_prime_2048 (BigNumber * ret = null);
		public static BigNumber get_rfc3526_prime_3072 (BigNumber * ret = null);
		public static BigNumber get_rfc3526_prime_4096 (BigNumber * ret = null);
		public static BigNumber get_rfc3526_prime_6144 (BigNumber * ret = null);
		public static BigNumber get_rfc3526_prime_8192 (BigNumber * ret = null);

		public bool is_zero ();

		public static int add (BigNumber r, BigNumber a, BigNumber b);
		public static int sub (BigNumber r, BigNumber a, BigNumber b);
		public static int mul (BigNumber r, BigNumber a, BigNumber b, BigNumberContext ctx);
		public static int mod (BigNumber rem, BigNumber m, BigNumber d, BigNumberContext ctx);
		public static int mod_exp (BigNumber r, BigNumber a, BigNumber p, BigNumber m, BigNumberContext ctx);

		public int num_bits ();
		public int num_bytes ();

		[CCode (cname = "BN_bn2bin")]
		public int to_big_endian ([CCode (array_length = false)] uint8[] to);

		[CCode (cname = "BN_bn2binpad")]
		public int to_big_endian_padded (uint8[] to);

		[CCode (cname = "BN_bn2dec")]
		public char * to_string ();

		[CCode (cname = "BN_bn2hex")]
		public char * to_hex_string ();
	}

	[CCode (cheader_filename = "openssl/crypto.h", lower_case_cprefix = "CRYPTO_")]
	namespace Crypto {
		public int memcmp (void * a, void * b, size_t len);
	}

	[CCode (cheader_filename = "openssl/err.h", lower_case_cprefix = "ERR_")]
	namespace Error {
		public ulong get_error ();
		public void error_string_n (ulong e, char[] buf);
	}

	public void free (void * addr);
}
