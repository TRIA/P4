#define V1MODEL_VERSION 20200408
#include <core.p4>
#if defined(__TARGET_TOFINO__)
#include <tna.p4>
#elif defined(__TARGET_PSA__)
#include <bmv2/psa.p4>
#else
#error Unsupported targed!
#endif

#include "proto.p4"
#include "headers.p4"
#include "util.p4"

error {
      Checksum
}

#ifndef __TOFINO_MODEL__

//#define Counter counter
//#define Checksum Checksum16

//typedef MulticastGroupUint_t MulticastGroupId_t;
//#define MulticastGroupId_t MulticastGroupUint_t
#endif

#define CPU_PORT 255
#define CPU_CLONE_SESSION_ID 99

// L3 Forwarding counters, for IP and RINA.
#define CNT_DROP 0
#define CNT_FWD  1
#define CNT_ERR  2

// Raw ingress packet counters
#define CNT_IN_IPV4   0
#define CNT_IN_IPV6   1
#define CNT_IN_EFCP   2
#define CNT_IN_ARP    3
#define CNT_IN_VLAN   4
#define CNT_IN_RINARP 5
#define CNT_IN_OTHER  6
#define CNT_IN_MAX    7

// Control that deals with changing the spoofed MACs for the right
// address so that the packet goes to the right place.
control SpoofMAC(inout mac_addr_t mac) {
    action spoof(mac_addr_t new_mac) {
        mac = new_mac;
    }

    table spoof_map {
        key = {
            mac: exact;
        }
        actions = {
            spoof;
        }
        size = 1024;
    }

    apply {
        spoof_map.apply();
    }
}

// Sets a destination MAC given a certain port. Used for VLANs, when
// the port the port is going through determines the next hop address.
#ifdef __TARGET_TOFINO__
 control PortToDMAC(
        in PortId_t dport,
        inout ethernet_h eth,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md) {
#else
control PortToDMAC(
        in PortId_t dport,
        inout ethernet_h eth,
        inout psa_egress_output_metadata_t ostd) {
#endif

    action drop() {
#ifdef __TARGET_TOFINO__
        eg_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif
    }

    action set_dmac(mac_addr_t dmac) {
        eth.dst_addr = dmac;
    }

    // Rewrite destination MAC depending on the outgoing port. This is
    // for multicasting in VLANs.
    table map {
        key = {
            dport: exact;
        }
        actions = {
            set_dmac;
            drop;
        }
        size = 512;
        default_action = drop;
    }

    apply {
        map.apply();
    }
}

// Sets an egress port given a destination Ethernet address. This is
// for regular unicast switching, and is to be done in ingress.
#ifdef __TARGET_TOFINO__
control DMACToPort(
        in ethernet_h eth,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
#else
control DMACToPort(
        in ethernet_h eth,
        inout psa_ingress_output_metadata_t ostd) {
#endif

    action drop() {
#ifdef __TARGET_TOFINO__
        ig_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif
    }

    action set_dport(PortId_t dport) {
#ifdef __TARGET_TOFINO__
        ig_intr_tm_md.ucast_egress_port = dport;
#else
        ostd.egress_port = dport;
#endif
    }

    table map {
        key = {
            eth.dst_addr: exact;
        }
        actions = {
            set_dport;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = NoAction;
    }

    apply {
        map.apply();
    }
}

// Select a destination MAC given a certain IP address.
#ifdef __TARGET_TOFINO__
control IPToDMAC(inout ethernet_h ethernet,
                 inout ipv4_h ipv4,
                 in    metadata_t ig_md,
                 in    ingress_intrinsic_metadata_t ig_intr_md,
                 inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
#else
control IPToDMAC(inout ethernet_h ethernet,
                 inout ipv4_h ipv4,
                 in    metadata_t ig_md,
                 inout psa_ingress_output_metadata_t ostd) {
#endif

#ifdef __TARGET_TOFINO__
    Counter<bit<32>, bit<2>>(2, CounterType_t.PACKETS) cnt;
#else
    Counter<bit<32>, bit<2>>(2, PSA_CounterType_t.PACKETS) cnt;
#endif

    action drop() {
#ifdef __TARGET_TOFINO__
        ig_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif

        cnt.count(CNT_DROP);
    }

    // IP forwarding action
    action forward(mac_addr_t dmac) {
        ethernet.src_addr = ethernet.dst_addr;
        ethernet.dst_addr = dmac;
        ipv4.ttl = ipv4.ttl - 1;

        cnt.count(CNT_FWD);
    }

    // Prefix IP routing
    table lpm {
        key = {
            ipv4.dst_addr: lpm;
        }
        actions = {
            forward;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = NoAction;
    }

    apply {
        if (ipv4.isValid()) {
            if (ig_md.checksum_error == 0) {
                lpm.apply();
            } else {
                drop();
            }
        } else {
            NoAction();
        }
    }
}

#ifdef __TARGET_TOFINO__
control RINAToDMAC(
        inout ethernet_h ethernet,
        inout efcp_h efcp,
        in    metadata_t ig_md,
        in    ingress_intrinsic_metadata_t ig_intr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
#else
control RINAToDMAC(
        inout ethernet_h ethernet,
        inout efcp_h efcp,
        in    metadata_t ig_md,
        inout psa_ingress_output_metadata_t ostd) {
#endif

#ifdef __TARGET_TOFINO__
    Counter<bit<32>, bit<2>>(2, CounterType_t.PACKETS) cnt;
#else
    Counter<bit<32>, bit<2>>(2, PSA_CounterType_t.PACKETS) cnt;
#endif

    action drop() {
#ifdef __TARGET_TOFINO__
        ig_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif

        cnt.count(CNT_DROP);
    }

    // EFCP forwarding action
    action forward(mac_addr_t dmac) {
        //hdr.dot1q.vlan_id = vlan_id;
        ethernet.src_addr = ethernet.dst_addr;
        ethernet.dst_addr = dmac;

        cnt.count(CNT_FWD);
    }

    table map {
        key = {
            efcp.dst_addr: exact;
        }
        actions = {
            forward;
            drop;
            NoAction;
        }
        size = 1024;
        default_action = NoAction();
    }

    apply {
        if (efcp.isValid()) {
            if (ig_md.checksum_error == 0) {
                map.apply();
            } else {
                drop();
            }
        } else {
            NoAction();
        }
    }
}

// Counter of the types of ingress packets we're seeing. This is
// mostly for diagnostic purposes.
control IngressCounter(in ether_type_t ether_type) {
#ifdef __TARGET_TOFINO__
    // Ingress packet counter.
    Counter<bit<32>, bit<3>>(CNT_IN_MAX, CounterType_t.PACKETS) per_type;

    // Raw ethernet counter.
    Counter<bit<32>, bit<1>>(1, CounterType_t.PACKETS) eth;
#else
    // Ingress packet counter.
    Counter<bit<32>, bit<3>>(CNT_IN_MAX, PSA_CounterType_t.PACKETS) per_type;

    // Raw ethernet counter.
    Counter<bit<32>, bit<1>>(1, PSA_CounterType_t.PACKETS) eth;
#endif

    apply {
        // Count inbound ethernet packets. The P4 compiler does not want us
        // to have this counter in the same counter object as the other.
        // Try it and you'll see!
        eth.count(0);

        // Count inbound packets by each type.
        if (ether_type == ETHERTYPE_IPV6) {
            per_type.count(CNT_IN_IPV6);
        } else if (ether_type == ETHERTYPE_IPV4) {
            per_type.count(CNT_IN_IPV4);
        } else if (ether_type == ETHERTYPE_EFCP) {
            per_type.count(CNT_IN_EFCP);
        } else if (ether_type == ETHERTYPE_ARP) {
            per_type.count(CNT_IN_ARP);
        } else if (ether_type == ETHERTYPE_DOT1Q) {
            per_type.count(CNT_IN_VLAN);
        } else if (ether_type == ETHERTYPE_RINARP) {
            per_type.count(CNT_IN_RINARP);
        } else {
            per_type.count(CNT_IN_OTHER);
        }
    }
}

// INGRESS

#if __TARGET_TOFINO__
parser SwitchIngressParser(packet_in buffer,
                           out header_t parsed_hdr,
                           inout metadata_t ig_md,
                           out ingress_intrinsic_metadata_t ig_intr_md,
                           out ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm) {
#else
parser LocalIngressParser(packet_in buffer,
                          out   header_t parsed_hdr,
                          inout metadata_t meta,
                          in    psa_ingress_parser_input_metadata_t istd,
                          in    empty_metadata_t resubmit_meta,
                          in    empty_metadata_t recirculate_meta) {
#endif
#ifdef __TARGET_TOFINO__
    TofinoIngressParser() tofino_parser;
    Checksum() ipv4_checksum;
    Checksum() efcp_checksum;
#else
    InternetChecksum() ipv4_checksum;
    InternetChecksum() efcp_checksum;
#endif

    state start {
#ifdef __TARGET_TOFINO__
        // Note: this may be enabling the parsing altogether
        // since, without it, the program will not work (tests will fail)
        tofino_parser.apply(packet, ig_intr_md);
#endif

        // Since there is nothing to parse for this header right now,
        // set it as valid and be done with it.
        parsed_hdr.bridged.setValid();

        // Save the ingress port immediately.
#ifdef __TARGET_TOFINO__
        parsed_hdr.bridged.ingress_port = ig_intr_md.ingress_port;
#else
        parsed_hdr.bridged.ingress_port = istd.ingress_port;
#endif

        transition parse_ethernet;
    }

    // Ethernet parsing
    state parse_ethernet {
        buffer.extract(parsed_hdr.ethernet);
        transition select (parsed_hdr.ethernet.ether_type) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            //default : reject;
        }
    }

    // VLAN packets parsing
    state parse_vlan {
        buffer.extract(parsed_hdr.dot1q);

        // Parse the inner packets..
        transition select(parsed_hdr.dot1q.ether_type) {
            EFCP_ETYPE: parse_efcp;
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_efcp {
        buffer.extract(parsed_hdr.efcp);
        efcp_checksum.add(parsed_hdr.efcp);
#ifdef __TARGET_TOFINO__
        ig_md.checksum_err_efcp_igprs = efcp_checksum.verify();
#else
        verify(efcp_checksum.get() == parsed_hdr.efcp.hdr_checksum, error.Checksum);
#endif

        // Validate the EFCP packet type.
        transition select(parsed_hdr.efcp.pdu_type) {
            DATA_TRANSFER:         accept;
            LAYER_MANAGEMENT:      accept;
            ACK_ONLY:              accept;
            NACK_ONLY:             accept;
            ACK_AND_FLOW_CONTROL:  accept;
            NACK_AND_FLOW_CONTROL: accept;
            FLOW_CONTROL_ONLY:     accept;
            CONTROL_ACK:           accept;
            RENDEVOUS:             accept;
            SELECTIVE_ACK:         accept;
            SELECTIVE_NACK:        accept;
            SELECTIVE_ACK_AND_FLOW_CONTROL:  accept;
            SELECTIVE_NACK_AND_FLOW_CONTROL: accept;
            //default: reject;
        }
   }

   state parse_ipv4 {
        buffer.extract(parsed_hdr.ipv4);
        ipv4_checksum.add(parsed_hdr.ipv4);
#ifdef __TARGET_TOFINO__
        ig_md.checksum_err_ipv4_igprs = ipv4_checksum.verify();
#else
        verify(efcp_checksum.get() == parsed_hdr.efcp.hdr_checksum, error.Checksum);
#endif
        transition accept;
    }
}

#if __TOFINO_MODEL__
control SwitchIngress(
        inout header_t hdr,
        inout metadata_t user_meta,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
#else
control LocalIngress(
        inout header_t hdr,
        inout metadata_t user_meta,
        in    psa_ingress_input_metadata_t istd,
        inout psa_ingress_output_metadata_t ostd) {
#endif
    DMACToPort() dmac_to_port;
    IPToDMAC() ip_to_dmac;
    RINAToDMAC() rina_to_dmac;
    IngressCounter() ingress_cnt;

    action drop() {
#ifdef __TARGET_TOFINO__
        ig_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif
    }

    // Ethernet broadcast. Don't configure this one if you use VLANs.
    action broadcast(bit<32> mcast_gid) {
#ifdef __TOFINO_MODEL__
        ig_intr_tm_md.mcast_grp_a = mcast_gid;
#else
        ostd.multicast_group = (MulticastGroup_t) mcast_gid;
#endif
        hdr.bridged.is_broadcast = 1;
    }

    // VLAN forwarding.
    action vlan_forward(MulticastGroup_t mcast_gid) {
#ifdef __TOFINO_MODEL__
        ig_intr_tm_md.mcast_grp_a = mcast_gid;
#else
        ostd.multicast_group = mcast_gid;
#endif

        hdr.bridged.is_vlan = 1;
    }

    table broadcast_map {
        key = {
            hdr.ethernet.dst_addr: ternary;
        }
        actions = {
            broadcast;
            NoAction;
        }
        size = 2;
        const default_action = NoAction();
        const entries = {
            // This is for broadcasts!
            0xFFFFFFFFFFFF &&& 0xFFFFFFFFFFFFF: broadcast(32w1);

            // This is the multicast mask.
            0x01005E000000 &&& 0x1FFFFFF000000: broadcast(32w1);
        }
    }

    // VLAN IDs
    table vlan_map {
        key = {
            hdr.dot1q.vlan_id: exact;
        }
        actions = {
            vlan_forward;
            NoAction;
        }
        size = 1024;
        const default_action = NoAction();
    }

    apply {
        // Deal with L2 routing if its configured.
        if (hdr.ethernet.isValid()) {

            ingress_cnt.apply(hdr.ethernet.ether_type);

            // VLAN routing.
            if (hdr.dot1q.isValid()) {
                vlan_map.apply();
            }
            // L3 routing otherwise.
            else {
                broadcast_map.apply();

#ifdef __TOFINO_MODEL__
                // RINA routing.
                rina_to_dmac.apply(hdr.ethernet, hdr.efcp, ig_md, ig_intr_md, ig_intr_dprsr_md);

                // IPv4 routing.
                ip_to_dmac.apply(hdr.ethernet, hdr.ipv4, ig_md, ig_intr_md, ig_intr_dprsr_md);

                // We should have an destination MAC address by this point...
                dmac_to_port.apply(hdr.ethernet, ig_intr_tm_md, ig_intr_dprsr_md);
#else
                // RINA routing.
                rina_to_dmac.apply(hdr.ethernet, hdr.efcp, user_meta, ostd);

                // IPv4 routing.
                ip_to_dmac.apply(hdr.ethernet, hdr.ipv4, user_meta, ostd);

                // We should have an destination MAC address by this point...
                dmac_to_port.apply(hdr.ethernet, ostd);
#endif
            }
        } else {
            // FIXME: Register an error here.
        }
    }
}

#ifdef __TARGET_TOFINO__
control SwitchIngressDeparser(
        packet_out buffer,
        inout header_t hdr) {
#else
control LocalIngressDeparser(
        packet_out buffer,
        out empty_metadata_t clone_i2e_meta,
        out empty_metadata_t resubmit_meta,
        out empty_metadata_t normal_meta,
        inout header_t hdr,
        in  metadata_t meta,
        in  psa_ingress_output_metadata_t istd) {
#endif
    apply {
        buffer.emit(hdr.bridged);
     	buffer.emit(hdr.ethernet);
        buffer.emit(hdr.dot1q);
        buffer.emit(hdr.efcp);
        buffer.emit(hdr.ipv4);
    }
}

// EGRESS

#ifdef __TOFINO_MODEL__
parser SwitchEgressParser(
        packet_in buffer,
        out header_t parsed_hdr,
        out egress_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
#else
parser LocalEgressParser(
        packet_in buffer,
        out   header_t parsed_hdr,
        inout metadata_t user_meta,
        in    psa_egress_parser_input_metadata_t istd,
        in    empty_metadata_t normal_meta,
        in    empty_metadata_t clone_i2e_meta,
        in    empty_metadata_t clone_e2e_meta) {
#endif

#ifdef __TOFINO_MODEL__
    TofinoEgressParser() tofino_parser;
#endif

    state start {
#ifdef __TOFINO_MODEL__
        tofino_parser.apply(packet, eg_intr_md);
#endif
        buffer.extract(parsed_hdr.bridged);
        transition parse_ethernet;
     }

    state parse_ethernet {
        buffer.extract(parsed_hdr.ethernet);
        transition select (parsed_hdr.ethernet.ether_type) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            //default : reject;
        }
    }

    state parse_vlan {
        buffer.extract(parsed_hdr.dot1q);
        transition select(parsed_hdr.dot1q.ether_type) {
            EFCP_ETYPE: parse_efcp;
            ETHERTYPE_IPV4: parse_ipv4;
            //default: reject;
        }
    }

    state parse_efcp {
        buffer.extract(parsed_hdr.efcp);
        transition select(parsed_hdr.efcp.pdu_type) {
            DATA_TRANSFER:         accept;
            LAYER_MANAGEMENT:      accept;
            ACK_ONLY:              accept;
            NACK_ONLY:             accept;
            ACK_AND_FLOW_CONTROL:  accept;
            NACK_AND_FLOW_CONTROL: accept;
            FLOW_CONTROL_ONLY:     accept;
            CONTROL_ACK:           accept;
            RENDEVOUS:             accept;
            SELECTIVE_ACK:         accept;
            SELECTIVE_NACK:        accept;
            SELECTIVE_ACK_AND_FLOW_CONTROL:  accept;
            SELECTIVE_NACK_AND_FLOW_CONTROL: accept;
            //default: reject;
        }
    }

    state parse_ipv4 {
        buffer.extract(parsed_hdr.ipv4);
        transition accept;
    }
}

#if __TOFINO_MODEL__
control SwitchEgress(
        inout header_t hdr,
        inout egress_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_prsr_md,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
#else
control LocalEgress(
        inout header_t hdr,
        inout metadata_t meta,
        in    psa_egress_input_metadata_t istd,
        inout psa_egress_output_metadata_t ostd) {
#endif
    SpoofMAC() dmac;
    SpoofMAC() smac;
    PortToDMAC() port_to_dmac;

    Checksum<bit<16>>(PSA_HashAlgorithm_t.ONES_COMPLEMENT16) ipv4_checksum;
    Checksum<bit<16>>(PSA_HashAlgorithm_t.ONES_COMPLEMENT16) efcp_checksum;


    action drop() {
#ifdef __TOFINO_MODEL__
        eg_intr_dprsr_md.drop_ctl = 0x1;
#else
        ostd.drop = true;
#endif
    }

    apply {
        // This is specific to VLANs. This prevents a multicast packet
        // from going back to its source port.
#ifdef __TOFINO_MODEL__
        if (hdr.bridged.ingress_port == eg_intr_md.egress_port) {
            drop();
        }
#else
        if (hdr.bridged.ingress_port == istd.egress_port) {
            drop();
        }
#endif

#ifdef __TOFINO_MODEL__
        // FIXME: This is for VLANs and I'm really not sure this is the right
        // way to proceed.
        if (hdr.bridged.is_vlan == 1) {
            port_to_dmac.apply(eg_intr_md.egress_port, hdr.ethernet, eg_intr_dprsr_md);
        }
#else
        if (hdr.bridged.is_vlan == 1) {
            port_to_dmac.apply(istd.egress_port, hdr.ethernet, ostd);
        }
#endif

        // Change the source MAC if necessary
        smac.apply(hdr.ethernet.src_addr);

        // Change the target MAC if necessary
        dmac.apply(hdr.ethernet.dst_addr);



        ipv4_checksum.update({
            hdr.ipv4.version,
            hdr.ipv4.ihl,
            hdr.ipv4.diffserv,
            hdr.ipv4.total_len,
            hdr.ipv4.identification,
            hdr.ipv4.flags,
            hdr.ipv4.frag_offset,
            hdr.ipv4.ttl,
            hdr.ipv4.protocol,
            hdr.ipv4.src_addr,
            hdr.ipv4.dst_addr});

        efcp_checksum.update({
            hdr.efcp.ver,
   	        hdr.efcp.dst_addr,
            hdr.efcp.src_addr,
            hdr.efcp.qos_id,
            hdr.efcp.dst_cep_id,
            hdr.efcp.src_cep_id,
            hdr.efcp.pdu_type,
            hdr.efcp.flags,
            hdr.efcp.len,
            hdr.efcp.seqnum});
    }
}

#ifdef __TOFINO_MODEL__
control SwitchEgressDeparser(
        packet_out packet,
        inout header_t hdr,
        in    egress_metadata_t eg_md,
        in    egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md) {
    Checksum() ipv4_checksum;
    Checksum() efcp_checksum;

    apply {
        if (hdr.ipv4.isValid()) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update({
                hdr.ipv4.version,
                hdr.ipv4.ihl,
                hdr.ipv4.diffserv,
                hdr.ipv4.total_len,
                hdr.ipv4.identification,
                hdr.ipv4.flags,
                hdr.ipv4.frag_offset,
                hdr.ipv4.ttl,
                hdr.ipv4.protocol,
                hdr.ipv4.src_addr,
                hdr.ipv4.dst_addr});
        }
        if (hdr.efcp.isValid()) {
            hdr.efcp.hdr_checksum = efcp_checksum.update({
                hdr.efcp.ver,
         	    hdr.efcp.dst_addr,
                hdr.efcp.src_addr,
                hdr.efcp.qos_id,
                hdr.efcp.dst_cep_id,
                hdr.efcp.src_cep_id,
                hdr.efcp.pdu_type,
                hdr.efcp.flags,
                hdr.efcp.len,
                hdr.efcp.seqnum});
        }

        packet.emit(hdr.ethernet);
        packet.emit(hdr.dot1q);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
    }
}
#else
// PSA
control LocalEgressDeparser(
        packet_out packet,
        out   empty_metadata_t clone_e2e_meta,
        out   empty_metadata_t recirculate_meta,
        inout header_t hdr,
        in    metadata_t meta,
        in    psa_egress_output_metadata_t istd,
        in    psa_egress_deparser_input_metadata_t edstd) {

    Checksum<bit<16>>(PSA_HashAlgorithm_t.ONES_COMPLEMENT16) ipv4_checksum;
    Checksum<bit<16>>(PSA_HashAlgorithm_t.ONES_COMPLEMENT16) efcp_checksum;

    apply {
        //ipv4_checksum.update({
        //    hdr.ipv4.version,
        //    hdr.ipv4.ihl,
        //    hdr.ipv4.diffserv,
        //    hdr.ipv4.total_len,
        //    hdr.ipv4.identification,
        //    hdr.ipv4.flags,
        //    hdr.ipv4.frag_offset,
        //    hdr.ipv4.ttl,
        //    hdr.ipv4.protocol,
        //    hdr.ipv4.src_addr,
        //    hdr.ipv4.dst_addr});
        //hdr.ipv4.hdr_checksum = ipv4_checksum.get();

        //efcp_checksum.add({
        //    hdr.efcp.ver,
   	    //    hdr.efcp.dst_addr,
        //    hdr.efcp.src_addr,
        //    hdr.efcp.qos_id,
        //    hdr.efcp.dst_cep_id,
        //    hdr.efcp.src_cep_id,
         //   hdr.efcp.pdu_type,
        //    hdr.efcp.flags,
        //    hdr.efcp.len,
        //    hdr.efcp.seqnum});
        //hdr.efcp.hdr_checksum = efcp_checksum.get();

        packet.emit(hdr.ethernet);
        packet.emit(hdr.dot1q);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
    }
}
#endif

#ifdef __TOFINO_MODEL__
Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;

#else
IngressPipeline(LocalIngressParser(),
                LocalIngress(),
                LocalIngressDeparser()) ingressPipeline;
EgressPipeline(LocalEgressParser(),
               LocalEgress(),
               LocalEgressDeparser()) egressPipeline;
PacketReplicationEngine() pre;
BufferingQueueingEngine() bqe;

PSA_Switch(ingressPipeline,
           pre,
           egressPipeline,
           bqe) main;
#endif
