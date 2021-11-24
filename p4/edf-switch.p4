#include <core.p4>
#include <tna.p4>

#include "proto.p4"
#include "headers.p4"
#include "util.p4"

#define CPU_PORT 255
#define CPU_CLONE_SESSION_ID 99

error {
    wrong_pdu_type
}

parser SwitchIngressParser(
         packet_in packet,
         out header_t hdr,
         out metadata_t ig_md,
         out ingress_intrinsic_metadata_t ig_intr_md,
         out ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm,
         out ingress_intrinsic_metadata_from_parser_t ig_intr_md_from_prsr) {
    TofinoIngressParser() tofino_parser;
    Checksum() ipv4_checksum;
    Checksum() efcp_checksum;

    state start {
        // Note: this may be enabling the parsing altogether
        // since, without it, the program will not work (tests will fail)
        tofino_parser.apply(packet, ig_intr_md);

        // Since there is nothing to parse for this header right now,
        // set it as valid and be done with it.
        hdr.bridged.setValid();

        // Save the ingress port immediately.
        hdr.bridged.ingress_port = ig_intr_md.ingress_port;

        transition parse_ethernet;
    }

    // Ethernet parsing
    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select (hdr.ethernet.ether_type) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            default : reject;
        }
    }

    // VLAN packets parsing
    state parse_vlan {
        packet.extract(hdr.dot1q);

        // Parse the inner packets..
        transition select(hdr.dot1q.ether_type) {
            EFCP_ETYPE: parse_efcp;
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        efcp_checksum.add(hdr.efcp);
        ig_md.checksum_err_efcp_igprs = efcp_checksum.verify();

        // Validate the EFCP packet type.
        transition select(hdr.efcp.pdu_type) {
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
            default: reject;
        }
   }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        ipv4_checksum.add(hdr.ipv4);
        ig_md.checksum_err_ipv4_igprs = ipv4_checksum.verify();
        transition accept;
    }
}

// Control that deals with changing the spoofed MACs for the right
// address so that the packet goes to the right place.
control TranslateMAC(inout mac_addr_t mac) {
    action translate(mac_addr_t new_mac) {
        mac = new_mac;
    }

    table changeTo {
        key = {
            mac: exact;
        }
        actions = {
            translate;
        }
        size = 1024;
    }

    apply {
        changeTo.apply();
    }
}

// Sets a destination MAC given a certain port. Used for VLANs, when
// the port the port is going through determines the next hop address.
control PortToDMAC(in PortId_t dport,
                   inout ethernet_h eth,
                   inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md) {
    action drop() {
        eg_intr_dprsr_md.drop_ctl = 0x1;
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
control DMACToPort(in ethernet_h eth,
                   inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md,
                   inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    action set_dport(PortId_t dport) {
        ig_intr_tm_md.ucast_egress_port = dport;
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
control IPToDMAC(inout ethernet_h ethernet,
                 inout ipv4_h ipv4,
                 in    metadata_t ig_md,
                 in    ingress_intrinsic_metadata_t ig_intr_md,
                 inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    // IP forwarding action
    action forward(mac_addr_t dmac) {
        ethernet.src_addr = ethernet.dst_addr;
        ethernet.dst_addr = dmac;
        ipv4.ttl = ipv4.ttl - 1;
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

control RINAToDMAC(inout ethernet_h ethernet,
                   inout efcp_h efcp,
                   in    metadata_t ig_md,
                   in    ingress_intrinsic_metadata_t ig_intr_md,
                   inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    // EFCP forwarding action
    action forward(mac_addr_t dmac) {
        //hdr.dot1q.vlan_id = vlan_id;
        ethernet.src_addr = ethernet.dst_addr;
        ethernet.dst_addr = dmac;
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

control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    DMACToPort() dmac_to_port;
    IPToDMAC() ip_to_dmac;
    RINAToDMAC() rina_to_dmac;

    // Counter<bit<32>, PortId_t>(64, CounterType_t.PACKETS_AND_BYTES) ipv4_counter;
    // Counter<bit<32>, PortId_t>(64, CounterType_t.PACKETS_AND_BYTES) efcp_counter;
    // Counter<bit<32>, PortId_t>(64, CounterType_t.PACKETS_AND_BYTES) eth_counter;

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    // VLAN forwarding.
    action vlan_forward(MulticastGroupId_t mcast_gid) {
        ig_intr_tm_md.mcast_grp_a = mcast_gid;

        hdr.bridged.is_replicated = 1;
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
        default_action = NoAction();
    }

    apply {
        // Deal with L2 routing if its configured.
        if (hdr.ethernet.isValid()) {

            // VLAN routing.
            if (hdr.dot1q.isValid()) {
                vlan_map.apply();
            }
            // L3 routing otherwise.
            else {

                // RINA routing.
                rina_to_dmac.apply(hdr.ethernet, hdr.efcp, ig_md, ig_intr_md, ig_intr_dprsr_md);

                // IPv4 routing.
                ip_to_dmac.apply(hdr.ethernet, hdr.ipv4, ig_md, ig_intr_md, ig_intr_dprsr_md);

                // We should have an destination MAC address by this point...
                dmac_to_port.apply(hdr.ethernet, ig_intr_tm_md, ig_intr_dprsr_md);
            }
        } else {
            // FIXME: Register an error here.
        }
    }
}


control SwitchIngressDeparser(
        packet_out packet,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    apply {
        packet.emit(hdr.bridged);
     	packet.emit(hdr.ethernet);
        packet.emit(hdr.dot1q);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
    }
}

parser SwitchEgressParser(
        packet_in packet,
        out header_t hdr,
        out egress_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {

    TofinoEgressParser() tofino_parser;

    state start {
        tofino_parser.apply(packet, eg_intr_md);
        packet.extract(hdr.bridged);
        transition parse_ethernet;
     }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select (hdr.ethernet.ether_type) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            default : reject;
        }
    }

    state parse_vlan {
        packet.extract(hdr.dot1q);
        transition select(hdr.dot1q.ether_type) {
            EFCP_ETYPE: parse_efcp;
            ETHERTYPE_IPV4: parse_ipv4;
            default: reject;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        transition select(hdr.efcp.pdu_type) {
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
            default: reject;
        }
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition accept;
    }
}

control SwitchEgress(
        inout header_t hdr,
        inout egress_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_prsr_md,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    TranslateMAC() dmac;
    TranslateMAC() smac;
    PortToDMAC() port_to_dmac;

    action drop() {
        eg_intr_dprsr_md.drop_ctl = 0x1;
    }

    apply {
        // This is specific to VLANs. This prevents a multicast packet
        // from going back to its source port.

        if (hdr.bridged.is_replicated == 1) {
            if (hdr.bridged.ingress_port == eg_intr_md.egress_port) {
                drop();
            } else {
                port_to_dmac.apply(eg_intr_md.egress_port, hdr.ethernet, eg_intr_dprsr_md);
            }
        }

        // Change the source MAC if necessary
        smac.apply(hdr.ethernet.src_addr);

        // Change the target MAC if necessary
        dmac.apply(hdr.ethernet.dst_addr);
    }
}

control SwitchEgressDeparser(
        packet_out packet,
        inout header_t hdr,
        in egress_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t eg_intr_dprsr_md) {

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



Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;

