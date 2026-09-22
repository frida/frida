[CCode (cheader_filename = "ngtcp2/ngtcp2.h", lower_case_cprefix = "ngtcp2_", gir_namespace = "NGTcp2", gir_version = "1.0")]
namespace NGTcp2 {
	[Compact]
	[CCode (cname = "ngtcp2_conn", cprefix = "ngtcp2_conn_", free_function = "ngtcp2_conn_del")]
	public class Connection {
		[CCode (cname = "ngtcp2_conn_client_new")]
		public static int make_client (out Connection conn, ConnectionID dcid, ConnectionID scid, Path path,
			ProtocolVersion client_chosen_version, Callbacks callbacks, Settings settings, TransportParams params,
			MemoryAllocator? mem, void * user_data);

		public void set_tls_native_handle (void * tls_native_handle);

		public void set_keep_alive_timeout (Duration timeout);

		public Timestamp get_expiry ();
		public int handle_expiry (Timestamp ts);

		[CCode (cname = "ngtcp2_conn_read_pkt")]
		public int read_packet (Path path, PacketInfo * pi, uint8[] pkt, Timestamp ts);

		public ssize_t write_connection_close (Path * path, PacketInfo * pi, uint8[] dest, ConnectionError error, Timestamp ts);

		public int open_bidi_stream (out int64 stream_id, void * stream_user_data);
		public int shutdown_stream (uint32 flags, int64 stream_id, uint64 app_error_code);

		public ssize_t write_stream (Path * path, PacketInfo * pi, uint8[] dest, ssize_t * pdatalen, WriteStreamFlags flags,
			int64 stream_id, uint8[]? data, Timestamp ts);
		public ssize_t writev_stream (Path * path, PacketInfo * pi, uint8[] dest, ssize_t * pdatalen, WriteStreamFlags flags,
			int64 stream_id, IOVector[] datav, Timestamp ts);

		public ssize_t write_datagram (Path * path, PacketInfo * pi, uint8[] dest, int * paccepted, uint32 flags, uint64 dgram_id,
			uint8[] data, Timestamp ts);
		public ssize_t writev_datagram (Path * path, PacketInfo * pi, uint8[] dest, int * paccepted, uint32 flags, uint64 dgram_id,
			IOVector[] datav, Timestamp ts);

		public uint64 get_max_data_left ();
		public uint64 get_max_stream_data_left (int64 stream_id);
	}

	public unowned string strerror (int liberr);

	[CCode (cname = "int", cprefix = "NGTCP2_ERR_", has_type_id = false)]
	public enum ErrorCode {
		INVALID_ARGUMENT,
		NOBUF,
		PROTO,
		INVALID_STATE,
		ACK_FRAME,
		STREAM_ID_BLOCKED,
		STREAM_IN_USE,
		STREAM_DATA_BLOCKED,
		FLOW_CONTROL,
		CONNECTION_ID_LIMIT,
		STREAM_LIMIT,
		FINAL_SIZE,
		CRYPTO,
		PKT_NUM_EXHAUSTED,
		REQUIRED_TRANSPORT_PARAM,
		MALFORMED_TRANSPORT_PARAM,
		FRAME_ENCODING,
		DECRYPT,
		STREAM_SHUT_WR,
		STREAM_NOT_FOUND,
		STREAM_STATE,
		RECV_VERSION_NEGOTIATION,
		CLOSING,
		DRAINING,
		TRANSPORT_PARAM,
		DISCARD_PKT,
		CONN_ID_BLOCKED,
		INTERNAL,
		CRYPTO_BUFFER_EXCEEDED,
		WRITE_MORE,
		RETRY,
		DROP_CONN,
		AEAD_LIMIT_REACHED,
		NO_VIABLE_PATH,
		VERSION_NEGOTIATION,
		HANDSHAKE_TIMEOUT,
		VERSION_NEGOTIATION_FAILURE,
		IDLE_CLOSE,
		FATAL,
		NOMEM,
		CALLBACK_FAILURE,
	}

	[CCode (cname = "ngtcp2_cid", has_destroy_function = false)]
	public struct ConnectionID {
		public size_t datalen;
		public uint8 data[MAX_CIDLEN];
	}

	[CCode (cname = "ngtcp2_ccerr", has_destroy_function = false)]
	public struct ConnectionError {
		[CCode (cname = "ngtcp2_ccerr_set_application_error")]
		public ConnectionError.application (uint64 error_code, uint8[]? reason = null);

		public ConnectionErrorType type;
		public uint64 error_code;
		public uint64 frame_type;
		[CCode (array_length_cname = "reasonlen")]
		public uint8[]? reason;
	}

	[CCode (cname = "ngtcp2_ccerr_type", cprefix = "NGTCP2_CCERR_TYPE_", has_type_id = false)]
	public enum ConnectionErrorType {
		TRANSPORT,
		APPLICATION,
		VERSION_NEGOTIATION,
		IDLE_CLOSE,
	}

	[CCode (cname = "ngtcp2_connection_id_status_type", cprefix = "NGTCP2_CONNECTION_ID_STATUS_TYPE_", has_type_id = false)]
	public enum ConnectionIdStatusType {
		ACTIVATE,
		DEACTIVATE,
	}

	[CCode (cname = "ngtcp2_path", has_destroy_function = false)]
	public struct Path {
		public Address local;
		public Address remote;
		public void * user_data;
	}

	[CCode (cname = "ngtcp2_path_validation_result", cprefix = "NGTCP2_PATH_VALIDATION_RESULT_", has_type_id = false)]
	public enum PathValidationResult {
		SUCCESS,
		FAILURE,
		ABORTED,
	}

	[CCode (cname = "uint32_t", cprefix = "NGTCP2_PROTO_VER_", has_type_id = false)]
	public enum ProtocolVersion {
		V1,
		V2,
		MIN,
		MAX,
	}

	[CCode (cname = "ngtcp2_pkt_info", has_destroy_function = false)]
	public struct PacketInfo {
		public uint8 ecn;
	}

	[CCode (cname = "ngtcp2_pkt_hd", has_destroy_function = false)]
	public struct PacketHeader {
		public ConnectionID dcid;
		public ConnectionID scid;
		public int64 pkt_num;
		[CCode (array_length_cname = "tokenlen")]
		public uint8[]? token;
		public size_t pkt_numlen;
		public size_t len;
		public uint32 version;
		public uint8 type;
		public uint8 flags;
	}

	[CCode (cname = "ngtcp2_pkt_stateless_reset", has_destroy_function = false)]
	public struct PacketStatelessReset {
		public uint8 stateless_reset_token[STATELESS_RESET_TOKENLEN];
		[CCode (array_length_cname = "randlen")]
		public uint8[] rand;
	}

	[Flags]
	[CCode (cname = "uint32_t", cprefix = "NGTCP2_WRITE_STREAM_FLAG_", has_type_id = false)]
	public enum WriteStreamFlags {
		NONE,
		MORE,
		FIN,
	}

	[Flags]
	[CCode (cname = "uint32_t", cprefix = "NGTCP2_WRITE_DATAGRAM_FLAG_", has_type_id = false)]
	public enum WriteDatagramFlags {
		NONE,
		MORE,
	}

	[CCode (cname = "NGTCP2_MAX_CIDLEN")]
	public const size_t MAX_CIDLEN;
	[CCode (cname = "NGTCP2_MIN_INITIAL_DCIDLEN")]
	public const size_t MIN_INITIAL_DCIDLEN;

	[CCode (cname = "NGTCP2_STATELESS_RESET_TOKENLEN")]
	public const size_t STATELESS_RESET_TOKENLEN;

	[CCode (cname = "ngtcp2_vec", has_destroy_function = false)]
	public struct IOVector {
		[CCode (array_length_cname = "len")]
		public unowned uint8[] base;
	}

	[CCode (cname = "ngtcp2_sa_family", cprefix = "NGTCP2_AF_", has_type_id = false)]
	public enum SocketAddressFamily {
		INET,
		INET6,
	}

	[SimpleType]
	[CCode (cname = "ngtcp2_in_port")]
	public struct InternetPort : uint16 {
	}

	[CCode (cname = "ngtcp2_sockaddr_union", has_destroy_function = false)]
	public struct SocketAddressUnion {
		public SocketAddress sa;
		public SocketAddressInternet in;
		public SocketAddressInternet6 in6;
	}

	[CCode (cname = "ngtcp2_sockaddr", has_destroy_function = false)]
	public struct SocketAddress {
		public SocketAddressFamily sa_family;
		public uint8 sa_data[14];
	}

	[CCode (cname = "ngtcp2_in_addr", has_destroy_function = false)]
	public struct InternetAddress {
		public uint32 s_addr;
	}

	[CCode (cname = "ngtcp2_sockaddr_in", has_destroy_function = false)]
	public struct SocketAddressInternet {
		public SocketAddressFamily sin_family;
		public InternetPort sin_port;
		public InternetAddress sin_addr;
		public uint8 sin_zero[8];
	}

	[CCode (cname = "ngtcp2_in6_addr", has_destroy_function = false)]
	public struct Internet6Address {
		public uint8 in6_addr[16];
	}

	[CCode (cname = "ngtcp2_sockaddr_in6", has_destroy_function = false)]
	public struct SocketAddressInternet6 {
		public SocketAddressFamily sin6_family;
		public InternetPort sin6_port;
		public uint32 sin6_flowinfo;
		public Internet6Address sin6_addr;
		public uint32 sin6_scope_id;
	}

	[SimpleType]
	[CCode (cname = "ngtcp2_socklen")]
	public struct SocketLength : uint32 {
	}

	[CCode (cname = "ngtcp2_addr", has_destroy_function = false)]
	public struct Address {
		[CCode (array_length_cname = "addrlen")]
		public uint8[] addr;
	}

	[CCode (cname = "ngtcp2_preferred_addr", has_destroy_function = false)]
	public struct PreferredAddress {
		public ConnectionID cid;
		public SocketAddressInternet ipv4;
		public SocketAddressInternet6 ipv6;
		public uint8 ipv4_present;
		public uint8 ipv6_present;
		public uint8 stateless_reset_token[STATELESS_RESET_TOKENLEN];
	}

	[CCode (cname = "ngtcp2_version_info", has_destroy_function = false)]
	public struct VersionInfo {
		public uint32 chosen_version;
		[CCode (array_length_cname = "available_versionslen")]
		public uint8[] available_versions;
	}

	[CCode (cname = "ngtcp2_encryption_level", cprefix = "NGTCP2_ENCRYPTION_LEVEL_", has_type_id = false)]
	public enum EncryptionLevel {
		INITIAL,
		HANDSHAKE,
		1RTT,
		0RTT,
	}

	[CCode (cname = "ngtcp2_token_type", cprefix = "NGTCP2_TOKEN_TYPE_", has_type_id = false)]
	public enum TokenType {
		UNKNOWN,
		RETRY,
		NEW_TOKEN,
	}

	[CCode (cname = "ngtcp2_rand_ctx", has_destroy_function = false)]
	public struct RNGContext {
		public void * native_handle;
	}

	[CCode (cname = "ngtcp2_crypto_aead", has_destroy_function = false)]
	public struct CryptoAead {
		public void * native_handle;
		public size_t max_overhead;
	}

	[CCode (cname = "ngtcp2_crypto_cipher", has_destroy_function = false)]
	public struct CryptoCipher {
		public void * native_handle;
	}

	[CCode (cname = "ngtcp2_crypto_aead_ctx", has_destroy_function = false)]
	public struct CryptoAeadCtx {
		public void * native_handle;
	}

	[CCode (cname = "ngtcp2_crypto_cipher_ctx", has_destroy_function = false)]
	public struct CryptoCipherCtx {
		public void * native_handle;
	}

	[CCode (cname = "ngtcp2_cc_algo", cprefix = "NGTCP2_CC_ALGO_", has_type_id = false)]
	public enum CongestionControlAlgorithm {
		RENO,
		CUBIC,
		BBR,
	}

	[SimpleType]
	[CCode (cname = "ngtcp2_tstamp")]
	public struct Timestamp : uint64 {
	}

	[CCode (cname = "NGTCP2_SECONDS")]
	public const uint64 SECONDS;

	[CCode (cname = "NGTCP2_MILLISECONDS")]
	public const uint64 MILLISECONDS;

	[CCode (cname = "NGTCP2_MICROSECONDS")]
	public const uint64 MICROSECONDS;

	[CCode (cname = "NGTCP2_NANOSECONDS")]
	public const uint64 NANOSECONDS;

	[SimpleType]
	[CCode (cname = "ngtcp2_duration")]
	public struct Duration : uint64 {
	}

	[CCode (cname = "ngtcp2_callbacks", has_destroy_function = false)]
	public struct Callbacks {
		public ClientInitial? client_initial;
		public RecvClientInitial? recv_client_initial;
		public RecvCryptoData recv_crypto_data;
		public HandshakeCompleted? handshake_completed;
		public RecvVersionNegotiation? recv_version_negotiation;
		public Encrypt encrypt;
		public Decrypt decrypt;
		public HpMask hp_mask;
		public RecvStreamData? recv_stream_data;
		public AckedStreamDataOffset? acked_stream_data_offset;
		public StreamOpen? stream_open;
		public StreamClose? stream_close;
		public RecvStatelessReset? recv_stateless_reset;
		public RecvRetry? recv_retry;
		public ExtendMaxStreams? extend_max_local_streams_bidi;
		public ExtendMaxStreams? extend_max_local_streams_uni;
		public Rand? rand;
		public GetNewConnectionId get_new_connection_id;
		public RemoveConnectionId? remove_connection_id;
		public UpdateKey update_key;
		public PathValidation? path_validation;
		public SelectPreferredAddr? select_preferred_addr;
		public StreamReset? stream_reset;
		public ExtendMaxStreams? extend_max_remote_streams_bidi;
		public ExtendMaxStreams? extend_max_remote_streams_uni;
		public ExtendMaxStreamData? extend_max_stream_data;
		public ConnectionIdStatus? dcid_status;
		public HandshakeConfirmed? handshake_confirmed;
		public RecvNewToken? recv_new_token;
		public DeleteCryptoAeadCtx delete_crypto_aead_ctx;
		public DeleteCryptoCipherCtx delete_crypto_cipher_ctx;
		public RecvDatagram? recv_datagram;
		public AckDatagram? ack_datagram;
		public LostDatagram? lost_datagram;
		public GetPathChallengeData get_path_challenge_data;
		public StreamStopSending? stream_stop_sending;
		public VersionNegotiation version_negotiation;
		public RecvKey recv_rx_key;
		public RecvKey recv_tx_key;
		public TlsEarlyDataRejected? tls_early_data_rejected;
	}

	[CCode (cname = "ngtcp2_client_initial", has_target = false)]
	public delegate int ClientInitial (Connection conn, void * user_data);
	[CCode (cname = "ngtcp2_recv_client_initial", has_target = false)]
	public delegate int RecvClientInitial (Connection conn, ConnectionID dcid, void * user_data);
	[CCode (cname = "ngtcp2_recv_crypto_data", has_target = false)]
	public delegate int RecvCryptoData (Connection conn, EncryptionLevel encryption_level, uint64 offset,
		[CCode (array_length_type = "size_t")] uint8[] data, void * user_data);
	[CCode (cname = "ngtcp2_handshake_completed", has_target = false)]
	public delegate int HandshakeCompleted (Connection conn, void * user_data);
	[CCode (cname = "ngtcp2_recv_version_negotiation", has_target = false)]
	public delegate int RecvVersionNegotiation (Connection conn, PacketHeader hd, [CCode (array_length_type = "size_t")] uint32[] sv,
		void * user_data);
	[CCode (cname = "ngtcp2_encrypt", has_target = false)]
	public delegate int Encrypt ([CCode (array_length = false)] uint8[] dest, CryptoAead aead, CryptoAeadCtx aead_ctx,
		[CCode (array_length_type = "size_t")] uint8[] plaintext,
		[CCode (array_length_type = "size_t")] uint8[] nonce,
		[CCode (array_length_type = "size_t")] uint8[] aad);
	[CCode (cname = "ngtcp2_decrypt", has_target = false)]
	public delegate int Decrypt ([CCode (array_length = false)] uint8[] dest, CryptoAead aead, CryptoAeadCtx aead_ctx,
		[CCode (array_length_type = "size_t")] uint8[] ciphertext,
		[CCode (array_length_type = "size_t")] uint8[] nonce,
		[CCode (array_length_type = "size_t")] uint8[] aad);
	[CCode (cname = "ngtcp2_hp_mask", has_target = false)]
	public delegate int HpMask ([CCode (array_length = false)] uint8[] dest, CryptoCipher hp, CryptoCipherCtx hp_ctx,
		[CCode (array_length = false)] uint8[] sample);
	[CCode (cname = "ngtcp2_recv_stream_data", has_target = false)]
	public delegate int RecvStreamData (Connection conn, uint32 flags, int64 stream_id, uint64 offset,
		[CCode (array_length_type = "size_t")] uint8[] data, void * user_data, void * stream_user_data);
	[CCode (cname = "ngtcp2_acked_stream_data_offset", has_target = false)]
	public delegate int AckedStreamDataOffset (Connection conn, int64 stream_id, uint64 offset, uint64 datalen, void * user_data,
		void * stream_user_data);
	[CCode (cname = "ngtcp2_stream_open", has_target = false)]
	public delegate int StreamOpen (Connection conn, int64 stream_id, void * user_data);
	[CCode (cname = "ngtcp2_stream_close", has_target = false)]
	public delegate int StreamClose (Connection conn, uint32 flags, int64 stream_id, uint64 app_error_code, void * user_data,
		void * stream_user_data);
	[CCode (cname = "ngtcp2_recv_stateless_reset", has_target = false)]
	public delegate int RecvStatelessReset (Connection conn, PacketStatelessReset sr, void * user_data);
	[CCode (cname = "ngtcp2_recv_retry", has_target = false)]
	public delegate int RecvRetry (Connection conn, PacketHeader hd, void * user_data);
	[CCode (cname = "ngtcp2_extend_max_streams", has_target = false)]
	public delegate int ExtendMaxStreams (Connection conn, uint64 max_streams, void * user_data);
	[CCode (cname = "ngtcp2_extend_max_stream_data", has_target = false)]
	public delegate int ExtendMaxStreamData (Connection conn, int64 stream_id, uint64 max_data, void * user_data,
		void * stream_user_data);
	[CCode (cname = "ngtcp2_rand", has_target = false)]
	public delegate void Rand ([CCode (array_length_type = "size_t")] uint8[] dest, RNGContext rand_ctx);
	[CCode (cname = "ngtcp2_get_new_connection_id", has_target = false)]
	public delegate int GetNewConnectionId (Connection conn, out ConnectionID cid, [CCode (array_length = false)] uint8[] token,
		size_t cidlen, void * user_data);
	[CCode (cname = "ngtcp2_remove_connection_id", has_target = false)]
	public delegate int RemoveConnectionId (Connection conn, ConnectionID cid, void * user_data);
	[CCode (cname = "ngtcp2_update_key", has_target = false)]
	public delegate int UpdateKey (Connection conn,
		[CCode (array_length = false)] uint8[] rx_secret,
		[CCode (array_length = false)] uint8[] tx_secret,
		CryptoAeadCtx rx_aead_ctx, [CCode (array_length = false)] uint8[] rx_iv,
		CryptoAeadCtx tx_aead_ctx, [CCode (array_length = false)] uint8[] tx_iv,
		[CCode (array_length_pos = 9.1)] uint8[] current_rx_secret,
		[CCode (array_length_pos = 9.1)] uint8[] current_tx_secret,
		void * user_data);
	[CCode (cname = "ngtcp2_path_validation", has_target = false)]
	public delegate int PathValidation (Connection conn, uint32 flags, Path path, Path old_path, PathValidationResult res,
		void * user_data);
	[CCode (cname = "ngtcp2_select_preferred_addr", has_target = false)]
	public delegate int SelectPreferredAddr (Connection conn, Path dest, PreferredAddress paddr, void * user_data);
	[CCode (cname = "ngtcp2_stream_reset", has_target = false)]
	public delegate int StreamReset (Connection conn, int64 stream_id, uint64 final_size, uint64 app_error_code, void * user_data,
		void * stream_user_data);
	[CCode (cname = "ngtcp2_connection_id_status", has_target = false)]
	public delegate int ConnectionIdStatus (Connection conn, ConnectionIdStatusType type, uint64 seq, ConnectionID cid,
		[CCode (array_length = false)] uint8[] token, void * user_data);
	[CCode (cname = "ngtcp2_handshake_confirmed", has_target = false)]
	public delegate int HandshakeConfirmed (Connection conn, void * user_data);
	[CCode (cname = "ngtcp2_recv_new_token", has_target = false)]
	public delegate int RecvNewToken (Connection conn, [CCode (array_length_type = "size_t")] uint8[] token, void * user_data);
	[CCode (cname = "ngtcp2_delete_crypto_aead_ctx", has_target = false)]
	public delegate void DeleteCryptoAeadCtx (Connection conn, CryptoAeadCtx aead_ctx, void * user_data);
	[CCode (cname = "ngtcp2_delete_crypto_cipher_ctx", has_target = false)]
	public delegate void DeleteCryptoCipherCtx (Connection conn, CryptoCipherCtx cipher_ctx, void * user_data);
	[CCode (cname = "ngtcp2_recv_datagram", has_target = false)]
	public delegate int RecvDatagram (Connection conn, uint32 flags, [CCode (array_length_type = "size_t")] uint8[] data,
		void * user_data);
	[CCode (cname = "ngtcp2_ack_datagram", has_target = false)]
	public delegate int AckDatagram (Connection conn, uint64 dgram_id, void * user_data);
	[CCode (cname = "ngtcp2_lost_datagram", has_target = false)]
	public delegate int LostDatagram (Connection conn, uint64 dgram_id, void * user_data);
	[CCode (cname = "ngtcp2_get_path_challenge_data", has_target = false)]
	public delegate int GetPathChallengeData (Connection conn, [CCode (array_length = false)] uint8[] data, void * user_data);
	[CCode (cname = "ngtcp2_stream_stop_sending", has_target = false)]
	public delegate int StreamStopSending (Connection conn, int64 stream_id, uint64 app_error_code, void * user_data,
		void * stream_user_data);
	[CCode (cname = "ngtcp2_version_negotiation", has_target = false)]
	public delegate int VersionNegotiation (Connection conn, uint32 version, ConnectionID client_dcid, void * user_data);
	[CCode (cname = "ngtcp2_recv_key", has_target = false)]
	public delegate int RecvKey (Connection conn, EncryptionLevel level, void * user_data);
	[CCode (cname = "ngtcp2_tls_early_data_rejected", has_target = false)]
	public delegate int TlsEarlyDataRejected (Connection conn, void * user_data);

	[CCode (cname = "ngtcp2_settings", has_destroy_function = false)]
	public struct Settings {
		[CCode (cname = "ngtcp2_settings_default")]
		public Settings.make_default ();

		public QlogWrite? qlog_write;
		public CongestionControlAlgorithm cc_algo;
		public Timestamp initial_ts;
		public Duration initial_rtt;
		public Printf? log_printf;
		public size_t max_tx_udp_payload_size;
		[CCode (array_length_cname = "tokenlen")]
		public uint8[]? token;
		public TokenType token_type;
		public RNGContext rand_ctx;
		public uint64 max_window;
		public uint64 max_stream_window;
		public size_t ack_thresh;
		public bool no_tx_udp_payload_size_shaping;
		public Duration handshake_timeout;
		[CCode (array_length_cname = "preferred_versionslen")]
		public ProtocolVersion[]? preferred_versions;
		[CCode (array_length_cname = "available_versionslen")]
		public ProtocolVersion[]? available_versions;
		public uint32 original_version;
		public bool no_pmtud;
		public uint32 initial_pkt_num;
	}

	[CCode (cname = "ngtcp2_qlog_write", has_target = false)]
	public delegate void QlogWrite (void * user_data, uint32 flags, [CCode (array_length_type = "size_t")] uint8[] data);
	[CCode (cname = "ngtcp2_printf", has_target = false)]
	public delegate void Printf (void * user_data, string format, ...);

	[CCode (cname = "ngtcp2_transport_params", has_destroy_function = false)]
	public struct TransportParams {
		[CCode (cname = "ngtcp2_transport_params_default")]
		public TransportParams.make_default ();

		public PreferredAddress preferred_addr;
		public ConnectionID original_dcid;
		public ConnectionID initial_scid;
		public ConnectionID retry_scid;
		public uint64 initial_max_stream_data_bidi_local;
		public uint64 initial_max_stream_data_bidi_remote;
		public uint64 initial_max_stream_data_uni;
		public uint64 initial_max_data;
		public uint64 initial_max_streams_bidi;
		public uint64 initial_max_streams_uni;
		public Duration max_idle_timeout;
		public uint64 max_udp_payload_size;
		public uint64 active_connection_id_limit;
		public uint64 ack_delay_exponent;
		public Duration max_ack_delay;
		public uint64 max_datagram_frame_size;
		public bool stateless_reset_token_present;
		public bool disable_active_migration;
		public bool original_dcid_present;
		public bool initial_scid_present;
		public bool retry_scid_present;
		public bool preferred_addr_present;
		public uint8 stateless_reset_token[STATELESS_RESET_TOKENLEN];
		public bool grease_quic_bit;
		public VersionInfo version_info;
		public bool version_info_present;
	}

	[CCode (cname = "ngtcp2_mem", has_destroy_function = false)]
	public struct MemoryAllocator {
		public void * user_data;
		public Malloc malloc;
		public Free free;
		public Calloc calloc;
		public Realloc realloc;
	}

	[CCode (cname = "ngtcp2_malloc", has_target = false)]
	public delegate void * Malloc (size_t size, void * user_data);
	[CCode (cname = "ngtcp2_free", has_target = false)]
	public delegate void Free (void * ptr, void * user_data);
	[CCode (cname = "ngtcp2_calloc", has_target = false)]
	public delegate void * Calloc (size_t nmemb, size_t size, void * user_data);
	[CCode (cname = "ngtcp2_realloc", has_target = false)]
	public delegate void * Realloc (void * ptr, size_t size, void * user_data);
}
