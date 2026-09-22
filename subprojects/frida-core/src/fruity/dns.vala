[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class DnsPacketReader {
		private BufferReader reader;

		public DnsPacketReader (Bytes packet) {
			reader = new BufferReader (new Buffer (packet, BIG_ENDIAN));
		}

		public DnsPtrRecord read_ptr () throws Error {
			var rr = read_record ();
			if (rr.key.type != PTR)
				throw new Error.PROTOCOL ("Expected a PTR record");
			if (rr.key.klass != IN)
				throw new Error.PROTOCOL ("Expected a PTR record of class IN");
			var subreader = new DnsPacketReader (rr.data);
			var name = subreader.read_name ();
			return new DnsPtrRecord () {
				key = rr.key,
				ttl = rr.ttl,
				data = rr.data,
				name = name,
			};
		}

		public DnsTxtRecord read_txt () throws Error {
			var rr = read_record ();
			if (rr.key.type != TXT)
				throw new Error.PROTOCOL ("Expected a TXT record");
			if (rr.key.klass != IN)
				throw new Error.PROTOCOL ("Expected a TXT record of class IN");
			var entries = new Gee.ArrayList<string> ();
			var subreader = new DnsPacketReader (rr.data);
			while (subreader.reader.available != 0) {
				string text = subreader.read_string ();
				entries.add (text);
			}
			return new DnsTxtRecord () {
				key = rr.key,
				ttl = rr.ttl,
				data = rr.data,
				entries = entries.to_array (),
			};
		}

		public DnsAaaaRecord read_aaaa () throws Error {
			var rr = read_record ();
			if (rr.key.type != AAAA)
				throw new Error.PROTOCOL ("Expected an AAAA record");
			if (rr.key.klass != IN)
				throw new Error.PROTOCOL ("Expected an AAAA record of class IN");
			var subreader = new DnsPacketReader (rr.data);
			unowned uint8[] raw_address = subreader.reader.read_data (16);
			var address = new InetAddress.from_bytes (raw_address, IPV6);
			return new DnsAaaaRecord () {
				key = rr.key,
				ttl = rr.ttl,
				data = rr.data,
				address = address,
			};
		}

		public DnsSrvRecord read_srv () throws Error {
			var rr = read_record ();
			if (rr.key.type != SRV)
				throw new Error.PROTOCOL ("Expected a SRV record");
			if (rr.key.klass != IN)
				throw new Error.PROTOCOL ("Expected a SRV record of class IN");
			var subreader = new DnsPacketReader (rr.data);
			var priority = subreader.reader.read_uint16 ();
			var weight = subreader.reader.read_uint16 ();
			var port = subreader.reader.read_uint16 ();
			var name = subreader.read_name ();
			return new DnsSrvRecord () {
				key = rr.key,
				ttl = rr.ttl,
				data = rr.data,
				priority = priority,
				weight = weight,
				port = port,
				name = name,
			};
		}

		public DnsResourceRecord read_record () throws Error {
			var key = read_key ();
			var ttl = reader.read_uint32 ();
			var size = reader.read_uint16 ();
			var data = reader.read_bytes (size);
			return new DnsResourceRecord () {
				key = key,
				ttl = ttl,
				data = data,
			};
		}

		public DnsResourceKey read_key () throws Error {
			var name = read_name ();
			var type = reader.read_uint16 ();
			var klass = reader.read_uint16 ();
			return new DnsResourceKey () {
				name = name,
				type = type,
				klass = klass,
			};
		}

		public string read_name () throws Error {
			var name = new StringBuilder.sized (256);

			while (true) {
				size_t size = reader.read_uint8 ();
				if (size == 0)
					break;
				if (size > 63)
					throw new Error.PROTOCOL ("Invalid DNS name length");

				var label = reader.read_fixed_string (size);
				if (name.len != 0)
					name.append_c ('.');
				name.append (label);
			}

			return name.str;
		}

		public string read_string () throws Error {
			size_t size = reader.read_uint8 ();
			return reader.read_fixed_string (size);
		}
	}

	public class DnsPtrRecord : DnsResourceRecord {
		public string name;
	}

	public class DnsTxtRecord : DnsResourceRecord {
		public string[] entries;
	}

	public class DnsAaaaRecord : DnsResourceRecord {
		public InetAddress address;
	}

	public class DnsSrvRecord : DnsResourceRecord {
		public uint16 priority;
		public uint16 weight;
		public uint16 port;
		public string name;
	}

	public class DnsResourceRecord {
		public DnsResourceKey key;
		public uint32 ttl;
		public Bytes data;
	}

	public class DnsResourceKey {
		public string name;
		public DnsRecordType type;
		public DnsRecordClass klass;
	}

	public enum DnsRecordType {
		PTR	= 12,
		TXT	= 16,
		AAAA	= 28,
		SRV	= 33,
	}

	public enum DnsRecordClass {
		IN = 1,
	}
}
