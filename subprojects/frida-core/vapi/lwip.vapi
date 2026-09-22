[CCode (gir_namespace = "LWIP", gir_version = "1.0")]
namespace LWIP {
	[CCode (cheader_filename = "lwip/tcpip.h", lower_case_cprefix = "tcpip_")]
	namespace Runtime {
		public void init (InitDoneFunc init_done);

		[CCode (cname = "tcpip_callback")]
		public ErrorCode schedule (WorkFunc work);

		[CCode (cname = "tcpip_init_done_fn")]
		public delegate void InitDoneFunc ();

		[CCode (cname = "tcpip_callback_fn")]
		public delegate void WorkFunc ();
	}

	[CCode (cheader_filename = "lwip/netif.h", cname = "struct netif", cprefix = "netif_")]
	public struct NetworkInterface {
		public static void add_noaddr (ref NetworkInterface netif, void * state, NetworkInterfaceInitFunc init,
			NetworkInterfaceInputFunc input = NetworkInterface.default_input_handler);

		public void remove ();

		public void set_up ();
		public void set_down ();

		public void set_link_up ();
		public void set_link_down ();

		public void ip6_addr_set (int8 addr_idx, IP6Address address);
		public void create_ip6_linklocal_address (bool from_mac_48bit);
		public ErrorCode add_ip6_address (IP6Address address, int8 * chosen_index = null);
		public void ip6_addr_set_state (int8 addr_index, IP6AddressState state);

		[CCode (cname = "netif_input")]
		public static ErrorCode default_input_handler (PacketBuffer pbuf, ref NetworkInterface netif);

		public IP6Address ip6_addr[IPV6_NUM_ADDRESSES];

		public NetworkInterfaceInputFunc input;
		public NetworkInterfaceLinkOutputFunc linkoutput;
		public NetworkInterfaceOutputIP6Func output_ip6;

		public void * state;

		public uint16 mtu;

		public uint8 hwaddr[MAX_HWADDR_LEN];
		public uint8 hwaddr_len;

		public NetworkInterfaceFlags flags;

		public uint8 num;

		public const uint8 MAX_HWADDR_LEN;
	}

	[CCode (cname = "netif_init_fn", has_target = false)]
	public delegate ErrorCode NetworkInterfaceInitFunc (ref NetworkInterface netif);

	[CCode (cname = "netif_input_fn", has_target = false)]
	public delegate ErrorCode NetworkInterfaceInputFunc (PacketBuffer pbuf, ref NetworkInterface netif);

	[CCode (cname = "netif_linkoutput_fn", has_target = false)]
	public delegate ErrorCode NetworkInterfaceLinkOutputFunc (ref NetworkInterface netif, PacketBuffer pbuf);

	[CCode (cname = "netif_output_ip6_fn", has_target = false)]
	public delegate ErrorCode NetworkInterfaceOutputIP6Func (ref NetworkInterface netif, PacketBuffer pbuf, IP6Address address);

	[Flags]
	[CCode (cheader_filename = "lwip/netif.h", cprefix = "NETIF_FLAG_")]
	public enum NetworkInterfaceFlags {
		UP,
		BROADCAST,
		LINK_UP,
		ETHARP,
		ETHERNET,
		IGMP,
		MLD6,
	}

	[CCode (cheader_filename = "lwip/prot/ethernet.h", lower_case_cprefix = "ETH_")]
	namespace Ethernet {
		public const uint8 HWADDR_LEN;

		namespace IPv6 {
			[CCode (cheader_filename = "lwip/ethip6.h", cname = "ethip6_output")]
			public ErrorCode output (ref NetworkInterface netif, PacketBuffer pbuf, IP6Address address);
		}
	}

	public const uint IPV6_NUM_ADDRESSES;

	[CCode (cheader_filename = "lwip/ip6_addr.h", cname = "ip6_addr_t", cprefix = "ip6_addr_")]
	public struct IP6Address {
		[CCode (cname = "ip6addr_aton")]
		public static IP6Address parse (string str);

		[CCode (cname = "ip6addr_ntoa_r")]
		public unowned string? to_string (char[] buf);

		public uint32 addr[4];
		public uint8 zone;
	}

	[Flags]
	[CCode (cheader_filename = "lwip/ip6_addr.h", cname = "u8_t", cprefix = "IP6_ADDR_", has_type_id = false)]
	public enum IP6AddressState {
		INVALID,
		TENTATIVE,
		TENTATIVE_1,
		TENTATIVE_2,
		TENTATIVE_3,
		TENTATIVE_4,
		TENTATIVE_5,
		TENTATIVE_6,
		TENTATIVE_7,
		VALID,
		PREFERRED,
		DEPRECATED,
		DUPLICATED,
	}

	[CCode (cheader_filename = "lwip/ip_addr.h", cname = "u8_t", cprefix = "IPADDR_TYPE_", has_type_id = false)]
	public enum IPAddressType {
		V4,
		V6,
		ANY,
	}

	[CCode (cheader_filename = "lwip/mld6.h", cname = "struct netif", cprefix = "netif_")]
	namespace IP6MulticastListenerDiscovery {
		[CCode (cname = "mld6_joingroup")]
		public ErrorCode join_group (IP6Address src_addr, IP6Address group_addr);
		[CCode (cname = "mld6_joingroup_netif")]
		public ErrorCode join_group_netif (ref NetworkInterface netif, IP6Address group_addr);
		[CCode (cname = "mld6_leavegroup")]
		public ErrorCode leave_group (IP6Address src_addr, IP6Address group_addr);
		[CCode (cname = "mld6_leavegroup_netif")]
		public ErrorCode leave_group_netif (ref NetworkInterface netif, IP6Address group_addr);
	}

	[Compact]
	[CCode (cheader_filename = "lwip/pbuf.h", cname = "struct pbuf", cprefix = "pbuf_")]
	public class PacketBuffer {
		public static PacketBuffer alloc (Layer layer, uint16 length, Type type);

		public PacketBuffer? next;
		[CCode (array_length_cname = "len")]
		public uint8[] payload;
		public uint16 tot_len;

		[CCode (array_length = false)]
		public unowned uint8[] get_contiguous (uint8[] buffer, uint16 len, uint16 offset = 0);

		public ErrorCode take (uint8[] data);

		[CCode (cname = "pbuf_layer", cprefix = "PBUF_", has_type_id = false)]
		public enum Layer {
			TRANSPORT,
			IP,
			LINK,
			RAW_TX,
			RAW,
		}

		[CCode (cname = "pbuf_type", cprefix = "PBUF_", has_type_id = false)]
		public enum Type {
			RAM,
			ROM,
			REF,
			POOL,
		}
	}

	[Compact]
	[CCode (cheader_filename = "lwip/tcp.h", cname = "struct tcp_pcb", cprefix = "tcp_")]
	public class TcpPcb {
		[CCode (cname = "tcp_new_ip_type")]
		public static unowned TcpPcb make (IPAddressType type);

		[CCode (cname = "tcp_arg")]
		public void set_user_data (void * user_data);

		[CCode (cname = "tcp_recv")]
		public void set_recv_callback (RecvFunc f);
		[CCode (cname = "tcp_sent")]
		public void set_sent_callback (SentFunc f);
		[CCode (cname = "tcp_poll")]
		public void set_poll_callback (PollFunc f, uint8 interval);
		[CCode (cname = "tcp_err")]
		public void set_error_callback (ErrorFunc f);

		public void set_flags (Flags flags);

		public void nagle_disable ();
		public void nagle_enable ();

		public void abort ();
		public ErrorCode close ();
		public ErrorCode shutdown (bool shut_rx, bool shut_tx);

		public void bind_netif (NetworkInterface * netif);

		public ErrorCode connect (IP6Address address, uint16 port, ConnectedFunc connected);

		[CCode (cname = "tcp_recved")]
		public void notify_received (uint16 len);

		[CCode (cname = "tcp_sndbuf")]
		public uint16 query_send_buffer_space ();

		[CCode (cname = "tcp_sndqueuelen")]
		public uint16 query_send_queue_length ();

		public ErrorCode write (uint8[] data, WriteFlags flags = 0);
		public ErrorCode output ();

		[CCode (cname = "tcp_recv_fn", has_target = false)]
		public delegate ErrorCode RecvFunc (void * user_data, TcpPcb pcb, owned PacketBuffer? pbuf, ErrorCode err);

		[CCode (cname = "tcp_sent_fn", has_target = false)]
		public delegate ErrorCode SentFunc (void * user_data, TcpPcb pcb, uint16 len);

		[CCode (cname = "tcp_poll_fn", has_target = false)]
		public delegate ErrorCode PollFunc (void * user_data, TcpPcb pcb);

		[CCode (cname = "tcp_err_fn", has_target = false)]
		public delegate void ErrorFunc (void * user_data, ErrorCode err);

		[CCode (cname = "tcp_connected_fn", has_target = false)]
		public delegate ErrorCode ConnectedFunc (void * user_data, TcpPcb pcb, ErrorCode err);

		[Flags]
		[CCode (cname = "tcpflags_t", cprefix = "TF_", has_type_id = false)]
		public enum Flags {
			ACK_DELAY,
			ACK_NOW,
			INFR,
			CLOSEPEND,
			RXCLOSED,
			FIN,
			NODELAY,
			NAGLEMEMERR,
			WND_SCALE,
			BACKLOGPEND,
			TIMESTAMP,
			RTO,
			SACK,
		}

		[Flags]
		[CCode (cname = "u8_t", cprefix = "TCP_WRITE_FLAG_", has_type_id = false)]
		public enum WriteFlags {
			COPY,
			MORE,
		}
	}

	namespace Tcp {
		[CCode (cname = "TCP_SNDLOWAT")]
		public const size_t SEND_LOW_WATERMARK;

		[CCode (cname = "TCP_SNDQUEUELOWAT")]
		public const size_t SEND_QUEUE_LOW_WATERMARK;
	}

	[Compact]
	[CCode (cheader_filename = "lwip/udp.h", cname = "struct udp_pcb", cprefix = "udp_")]
	public class UdpPcb {
		[CCode (cname = "udp_new_ip_type")]
		public static unowned UdpPcb make (IPAddressType type);

		public void remove ();

		[CCode (cname = "udp_recv")]
		public void set_recv_callback (RecvFunc f);

		public ErrorCode bind (IP6Address? ipaddr = null, uint16 port = 0);
		public void bind_netif (NetworkInterface * netif);

		public ErrorCode connect (IP6Address ipaddr, uint16 port);

		public ErrorCode send (PacketBuffer pbuf);
		public ErrorCode sendto (PacketBuffer pbuf, IP6Address dst_ip, uint16 dst_port);

		[CCode (cname = "tcp_recv_fn", instance_pos = 0)]
		public delegate void RecvFunc (UdpPcb pcb, owned PacketBuffer? pbuf, IP6Address addr, uint16 port);

		public IP6Address local_ip;
		public IP6Address remote_ip;
		public uint8 netif_idx;
		public uint8 so_options;
		public uint8 tos;
		public uint8 ttl;

		public uint8 flags;
		public uint16 local_port;
		public uint16 remote_port;

		public uint8 mcast_ifindex;
		public uint8 mcast_ttl;
	}

	[CCode (cheader_filename = "lwip/err.h", cname = "err_t", cprefix = "ERR_", lower_case_cprefix = "err_", has_type_id = false)]
	public enum ErrorCode {
		OK,
		MEM,
		BUF,
		TIMEOUT,
		RTE,
		INPROGRESS,
		VAL,
		WOULDBLOCK,
		USE,
		ALREADY,
		ISCONN,
		CONN,
		IF,
		ABRT,
		RST,
		CLSD,
		ARG;

		public int to_errno ();
	}
}
