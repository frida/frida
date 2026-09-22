[CCode (cheader_filename = "windns.h", gir_namespace = "Windows", gir_version = "1.0")]
namespace WinDns {
	[CCode (cname = "DNS_QUERY_RESULT")]
	public struct QueryResult {
		[CCode (cname = "Version")]
		public ulong version;
		[CCode (cname = "QueryStatus")]
		public long query_status;
		[CCode (cname = "QueryOptions")]
		public uint64 query_options;
		[CCode (cname = "pQueryRecords")]
		public Record * query_records;
		[CCode (cname = "Reserved")]
		public void * reserved;
	}

	[CCode (cname = "DNS_RECORDW")]
	public struct Record {
		[CCode (cname = "pNext")]
		public Record * next;
		[CCode (cname = "pName")]
		public unowned string16 name;
		[CCode (cname = "wType")]
		public RecordType type;
		[CCode (cname = "wDataLength")]
		public uint16 data_length;
		[CCode (cname = "Flags.DW")]
		public uint32 flags;
		[CCode (cname = "dwTtl")]
		public uint32 ttl;
		[CCode (cname = "dwReserved")]
		public uint32 reserved;

		[CCode (cname = "Data.A")]
		public AData a;
		[CCode (cname = "Data.PTR")]
		public PtrData ptr;
		[CCode (cname = "Data.TXT")]
		public TxtData txt;
		[CCode (cname = "Data.AAAA")]
		public AaaaData aaaa;
		[CCode (cname = "Data.SRV")]
		public SrvData srv;
	}

	[CCode (cname = "WORD", cprefix = "DNS_TYPE_", has_type_id = false)]
	public enum RecordType {
		A,
		PTR,
		TEXT,
		AAAA,
		SRV,
	}

	[CCode (cname = "DNS_A_DATA")]
	public struct AData {
		[CCode (cname = "IpAddress")]
		public uint32 ip;
	}

	[CCode (cname = "DNS_PTR_DATAW")]
	public struct PtrData {
		[CCode (cname = "pNameHost")]
		public unowned string16 name;
	}

	[CCode (cname = "DNS_TXT_DATAW")]
	public struct TxtData {
		[CCode (cname = "pStringArray", array_length_cname = "dwStringCount")]
		public unowned string16[] strings;
	}

	[CCode (cname = "DNS_AAAA_DATA")]
	public struct AaaaData {
		[CCode (cname = "Ip6Address")]
		public IP6Address ip;
	}

	[CCode (cname = "DNS_SRV_DATAW")]
	public struct SrvData {
		[CCode (cname = "pNameTarget")]
		public unowned string16 name_target;
		[CCode (cname = "wPriority")]
		public uint16 priority;
		[CCode (cname = "wWeight")]
		public uint16 weight;
		[CCode (cname = "wPort")]
		public uint16 port;
	}

	[CCode (cname = "IP6_ADDRESS")]
	public struct IP6Address {
		[CCode (cname = "IP6Byte")]
		public uint8 data[16];
	}
}
