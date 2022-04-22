#define V1MODEL_VERSION 20200408

#include <core.p4>
#include <v1model.p4>

#include "proto.p4"
#include "headers.p4"
#include "util.p4"

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


// #########

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
control PortToDMAC(in PortId_t dport,
                   inout ethernet_h eth,
                   inout standard_metadata_t standard_metadata) {
    action drop() {
        mark_to_drop(standard_metadata);
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
                   inout standard_metadata_t standard_metadata) {
    action drop() {
        mark_to_drop(standard_metadata);
    }

    action set_dport(PortId_t dport) {
        standard_metadata.egress_spec = dport;
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
                 inout standard_metadata_t standard_metadata) {

    counter<bit<2>>(2, CounterType.packets) cnt;

    action drop() {
        mark_to_drop(standard_metadata);

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
    table ip2mac {
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
            if (standard_metadata.checksum_error == 0) {
                ip2mac.apply();
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
                   inout standard_metadata_t standard_metadata) {

    counter<bit<2>>(2, CounterType.packets) cnt;

    action drop() {
        mark_to_drop(standard_metadata);

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
            if (standard_metadata.checksum_error == 0) {
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
    // Ingress packet counter.
    counter<bit<3>>(CNT_IN_MAX, CounterType.packets) per_type;

    // Raw ethernet counter.
    counter<bit<1>>(1, CounterType.packets) eth;

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

// V1MODEL 

parser LocalParser(packet_in packet,
                   out header_t hdr,
                   inout metadata_t meta,
                   inout standard_metadata_t standard_metadata) {
    state start {
        // Since there is nothing to parse for this header right now,
        // set it as valid and be done with it.
        hdr.bridged.setValid();

        // Save the ingress port immediately.
        hdr.bridged.ingress_port = standard_metadata.ingress_port;

        transition parse_ethernet;
    }

    // Ethernet parsing
    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select (hdr.ethernet.ether_type) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
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
        }
   }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition accept;
    }
}

control LocalVerifyChecksum(inout header_t hdr, inout metadata_t meta) {
    apply {
    
    }
}

control LocalIngress(inout header_t hdr,
                     inout metadata_t meta,
                     inout standard_metadata_t standard_metadata) {
    DMACToPort() dmac_to_port;
    IPToDMAC() ip_to_dmac;
    RINAToDMAC() rina_to_dmac;
    IngressCounter() ingress_cnt;

    action drop() {
        mark_to_drop(standard_metadata);
    }

    // Ethernet broadcast. Don't configure this one if you use VLANs,
    // as it will not work as you expect. This might be due to my
    // implemention of VLANs here being iffy. I've tried looking for a
    // reference implementation written in P4 but could not find a simple
    // implementation I could partially reuse. The default "switch.p4" is
    // fairly complex.
    action broadcast(bit<16> mcast_gid) {
        standard_metadata.mcast_grp = mcast_gid;

        hdr.bridged.is_broadcast = 1;
    }

    // VLAN forwarding. This maps a switch port to a multicast group.
    action vlan_forward(bit<16> mcast_gid) {
        standard_metadata.mcast_grp = mcast_gid;

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
        const default_action = NoAction();
        const entries = {
            // This is for broadcasts!
            0xFFFFFFFFFFFF &&& 0xFFFFFFFFFFFF: broadcast(1);

            // This is the multicast mask.
            0x01005E000000 &&& 0x1FFFFFF00000: broadcast(1);
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

                // The 2 following actions were NOT tested on a real
                // network. They work with the Tofino model, but it is
                // not clear they make sense on an actual network. In any
                // case, they are an interesting learning exercise.

                // RINA routing.
                rina_to_dmac.apply(hdr.ethernet, hdr.efcp, standard_metadata);

                // IPv4 routing.
                ip_to_dmac.apply(hdr.ethernet, hdr.ipv4, standard_metadata);

                // Back here we're back in the normal operation of a
                // L2 switch.

                // We should have an destination MAC address by this point...
                dmac_to_port.apply(hdr.ethernet, standard_metadata);
            }
        } else {
            // FIXME: Register an error here although it's really not
            // clear how we'll get there. How will a bad Ethernet will
            // reach this point, really?
        }
    }
}

control LocalEgress(inout header_t hdr,
                    inout metadata_t meta,
                    inout standard_metadata_t standard_metadata) {
    SpoofMAC() dmac;
    SpoofMAC() smac;
    PortToDMAC() port_to_dmac;

    action drop() {
        mark_to_drop(standard_metadata);
    }

    apply {
        // This is specific to VLANs. This prevents a multicast packet
        // from going back to its source port.
        if (hdr.bridged.ingress_port == standard_metadata.egress_port) {
            drop();
        }

        // FIXME: This is for VLANs and I'm really not sure this is the right
        // way to proceed.
        if (hdr.bridged.is_vlan == 1) {
            port_to_dmac.apply(standard_metadata.egress_port, hdr.ethernet, standard_metadata);
        }

        // Change the source MAC if necessary
        smac.apply(hdr.ethernet.src_addr);

        // Change the target MAC if necessary
        dmac.apply(hdr.ethernet.dst_addr);
    }
}

control LocalComputeChecksum(inout header_t hdr,
                             inout metadata_t meta) {
    apply {
    }
}

control LocalDeparser(packet_out packet,
                      in header_t hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.dot1q);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
    }
}

V1Switch(
    LocalParser(),
    LocalVerifyChecksum(),
    LocalIngress(),
    LocalEgress(),
    LocalComputeChecksum(),
    LocalDeparser()
) main;