#ifndef _HEADERS_
#define _HEADERS_

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<128> ipv6_addr_t;
typedef bit<12> vlan_id_t;
typedef bit<9> egress_spec;

typedef bit<16> ether_type_t;
const ether_type_t ETHERTYPE_ARP = 16w0x0806;
const ether_type_t ETHERTYPE_EFCP = 16w0xD1F;
const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;
const ether_type_t ETHERTYPE_IPV6 = 16w0x86dd;
const ether_type_t ETHERTYPE_DOT1Q = 16w0x8100;

typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_ICMP = 1;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;

/*
 * PDU Types
 */
const bit<8> DATA_TRANSFER = 0x80;
const bit<8> LAYER_MANAGEMENT = 0x40;
const bit<8> ACK_ONLY = 0xC1;
const bit<8> NACK_ONLY = 0xC2;
const bit<8> ACK_AND_FLOW_CONTROL = 0xC5;
const bit<8> NACK_AND_FLOW_CONTROL = 0xC6;
const bit<8> FLOW_CONTROL_ONLY = 0xC4;
const bit<8> SELECTIVE_ACK = 0xC9;
const bit<8> SELECTIVE_NACK = 0xCA;
const bit<8> SELECTIVE_ACK_AND_FLOW_CONTROL = 0xCD;
const bit<8> SELECTIVE_NACK_AND_FLOW_CONTROL = 0xCE;
const bit<8> CONTROL_ACK = 0xC0;
const bit<8> RENDEVOUS = 0xCF;

/*
 * ECN threshold for congestion control
 */
const bit<19> ECN_THRESHOLD = 1;

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    ether_type_t ether_type;
}

/*
  Dot1Q / 802.1Q tag format

  prio : BitField(3)
  id   : BitField(1)
  vlan : BitField(12)
  type : BitField(16)
*/
header dot1q_h {
    bit<3> prio;
    bit<1> is_drop;
    vlan_id_t vlan_id;
    ether_type_t proto_id;
}

header efcp_h {
    bit<8>  ver;
    bit<16> dst_addr; 
    bit<16> src_addr;
    bit<8>  qos_id;
    bit<16> dst_cep_id;
    bit<16> src_cep_id;
    bit<8> pdu_type;
    bit<8> flags;
    bit<16> len;
    bit<32> seqnum;
    bit<16> hdr_checksum;
}

header mpls_h {
    bit<20> label;
    bit<3> exp;
    bit<1> bos;
    bit<8> ttl;
}

header ipv4_h {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<13> frag_offset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header ipv6_h {
    bit<4> version;
    bit<8> traffic_class;
    bit<20> flow_label;
    bit<16> payload_len;
    bit<8> next_hdr;
    bit<8> hop_limit;
    ipv6_addr_t src_addr;
    ipv6_addr_t dst_addr;
}

header tcp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4> data_offset;
    bit<4> res;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> hdr_length;
    bit<16> checksum;
}

header icmp_h {
    bit<8> type_;
    bit<8> code;
    bit<16> hdr_checksum;
}

header arp_h {
    bit<16> hw_type;
    bit<16> proto_type;
    bit<8> hw_addr_len;
    bit<8> proto_addr_len;
    bit<16> opcode;
    // ...
}

// Segment Routing Extension (SRH) -- IETFv7
header ipv6_srh_h {
    bit<8> next_hdr;
    bit<8> hdr_ext_len;
    bit<8> routing_type;
    bit<8> seg_left;
    bit<8> last_entry;
    bit<8> flags;
    bit<16> tag;
}

// VXLAN -- RFC 7348
header vxlan_h {
    bit<8> flags;
    bit<24> reserved;
    bit<24> vni;
    bit<8> reserved2;
}

// Generic Routing Encapsulation (GRE) -- RFC 1701
header gre_h {
    bit<1> C;
    bit<1> R;
    bit<1> K;
    bit<1> S;
    bit<1> s;
    bit<3> recurse;
    bit<5> flags;
    bit<3> version;
    bit<16> proto;
}

struct egress_metadata_t {
    bool checksum_pdu_efcp;
    bool checksum_pdu_ipv4;

    bool checksum_err_efcp_egprs;
    bool checksum_err_ipv4_egprs;

    bit<19> enq_qdepth;
}

struct metadata_t {
    bool checksum_err_efcp_igprs;
    bool checksum_err_ipv4_igprs;

    bit<16> checksum_error;
}


struct header_t {
    ethernet_h  ethernet;
    dot1q_h     dot1q;
    efcp_h      efcp;
    ipv4_h      ipv4;
}

struct empty_header_t {}

struct empty_metadata_t {}

#endif /* _HEADERS_ */

