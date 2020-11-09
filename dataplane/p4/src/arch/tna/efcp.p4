/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2019-present Barefoot Networks, Inc.
 *
 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks, Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination of
 * this information or reproduction of this material is strictly forbidden unless
 * prior written permission is obtained from Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a written
 * agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/

#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

/*
 * All headers, used in the program needs to be assembed into a single struct.
 * We only need to declare the type, but there is no need to instantiate it,
 * because it is done "by the architecture", i.e. outside of P4 functions. It is 
 * allocated in headers.p4
 */
#include "../../common/headers.p4"
#include "../../common/util.p4"


// NOTE: consider reviewing whether something can be moved into headers.p4

struct egress_metadata_t {
    // TODO: Check if all are needed
    bit<16> checksum_efcp_tmp;
    bit<16> checksum_ipv4_tmp;

    bool checksum_pdu_efcp;
    bool checksum_pdu_ipv4;

    bool checksum_err_efcp_igprs;
    bool checksum_err_ipv4_igprs;
    // NOTE: egress checksum error flags may not be needed
    // if we remove that from the egress pipeline
    bool checksum_err_efcp_egprs;
    bool checksum_err_ipv4_egprs;
 
    bit<19> enq_qdepth;
    //srv6_metadata_t srv6;
}

/*
 * All metadata, globally used in the program, also needs to be assembed
 * into a single struct. As in the case of the headers, we only need to
 * declare the type, but there is no need to instantiate it,
 * because it is done "by the architecture", i.e. outside of P4 functions
*/
//struct metadata_t {}
struct metadata_t {
    // TODO: Check if all are needed
    bit<16> checksum_efcp_tmp;
    bit<16> checksum_ipv4_tmp;
    bool checksum_pdu_efcp;
    bool checksum_pdu_ipv4;
    bool checksum_err_efcp_igprs;
    bool checksum_err_ipv4_igprs;
    // NOTE: egress checksum error flags may not be needed
    // if we remove that from the egress pipeline
    bool checksum_err_efcp_egprs;
    bool checksum_err_ipv4_egprs;
 
    bit<16> checksum_error;
    bit<16> parser_error;
    bit<9>  egress_spec;
}


/*************************************************************************
*********************** C O N S T A N T S ********************************
*************************************************************************/

/*
CPU_PORT specifies the P4 port number associated to packet-in and packet-out.
All packets forwarded via this port will be delivered to the controller as
PacketIn messages. Similarly, PacketOut messages from the controller will be
seen by the P4 pipeline as coming from the CPU_PORT.
*/
#define CPU_PORT 255

/*
CPU_CLONE_SESSION_ID specifies the mirroring session for packets to be cloned
to the CPU port. Packets associated with this session ID will be cloned to
the CPU_PORT as well as being transmitted via their egress port as set by the
bridging/routing/acl table. For cloning to work, the P4Runtime controller
needs first to insert a CloneSessionEntry that maps this session ID to the
CPU_PORT.
*/
#define CPU_CLONE_SESSION_ID 99

// Declare user-defined errors that may be signaled during parsing
error {
    wrong_pdu_type
}


/***************************************************************************************
*********************** I N G R E S S   P A R S E R  ***********************************
***************************************************************************************/

parser SwitchIngressParser(
        packet_in packet,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    Checksum() efcp_checksum;
    Checksum() ipv4_checksum;

    TofinoIngressParser() tofino_parser;

    state start {
        // Note: this may be enabling the parsing altogether
        // since, without it, the program will not work (tests will fail)
        tofino_parser.apply(packet, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select (hdr.ethernet.ether_type) {
            ETHERTYPE_EFCP: parse_efcp;
            ETHERTYPE_VLAN: parse_vlan;
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.ether_type) {
            ETHERTYPE_EFCP: parse_efcp;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        efcp_checksum.add(hdr.efcp);
        ig_md.checksum_err_efcp_igprs = efcp_checksum.verify();
	transition accept;
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        ipv4_checksum.add(hdr.ipv4);
        ig_md.checksum_err_ipv4_igprs = ipv4_checksum.verify();
        transition accept;
    }
}


/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {

    Counter<bit<32>, PortId_t>(512, CounterType_t.PACKETS_AND_BYTES) ipv4_counter;
    Counter<bit<32>, PortId_t>(512, CounterType_t.PACKETS_AND_BYTES) efcp_counter;

    Alpm(number_partitions = 1024, subtrees_per_partition = 2) algo_lpm;

    /*
     * Drop action
     */
    action miss() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    /*
     * EFCP forwarding action
     */
    action efcp_forward(bit<12> vlan_id, mac_addr_t dst_mac, egress_spec dst_port) {
        hdr.vlan.vlan_id = vlan_id;
        ig_intr_tm_md.ucast_egress_port = dst_port;
        hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
        hdr.ethernet.dst_addr = dst_mac;
	ig_intr_dprsr_md.drop_ctl = 0x0;
	efcp_counter.count(dst_port);
    }

    /*
     * IPv4 forwarding action
     */
    action ipv4_forward(mac_addr_t src_mac, mac_addr_t dst_mac, PortId_t dst_port) {
        ig_intr_tm_md.ucast_egress_port = dst_port;
        hdr.ethernet.dst_addr = dst_mac;
        hdr.ethernet.src_addr = src_mac;
        ig_intr_dprsr_md.drop_ctl = 0x0;
    	ipv4_counter.count(dst_port);
    }

    /*
     * EFCP exact table
     */
    table efcp_exact {
        key = {
            hdr.efcp.dst_addr: exact;
        }
        actions = {
            efcp_forward;
	    miss;
            NoAction;
        }
        size = 1024;
        //default_action = NoAction();
    }

    /*
     * IPv4 long prefix match table
     */
    table ipv4_lpm {
        key = {
            hdr.ipv4.dst_addr : lpm;
        }

        actions = {
            ipv4_forward;
            miss;
            NoAction;
        }

        size = 1024;
        alpm = algo_lpm;
    }

    apply {
        if (hdr.efcp.isValid() && ig_md.checksum_error == 0) {
           efcp_exact.apply();     
        } else if (hdr.ipv4.isValid() && ig_md.checksum_error == 0) {
            ipv4_lpm.apply();
        } else {
            miss();
        }
    }
}


/******************************************************************************************
***********************  I N G R E S S     D E P A R S E R  *******************************
******************************************************************************************/

control SwitchIngressDeparser(
        packet_out packet,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    apply {
        packet.emit(hdr);
	//packet.emit(hdr.ethernet);
        //packet.emit(hdr.efcp);
        //packet.emit(hdr.ipv4);
        //packet.emit(hdr.vlan);
    }
}

/*************************************************************************************
*********************** E G R E S S   P A R S E R  ***********************************
*************************************************************************************/

parser SwitchEgressParser(
        packet_in packet,
        out header_t hdr,
        out egress_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
       
    // Checksum() efcp_checksum;
    // Checksum() ipv4_checksum;

    TofinoEgressParser() tofino_parser;

    state start {
        // Note: this may be enabling the parsing altogether
        // since, without it, the program will not work (tests will fail)
        tofino_parser.apply(packet, eg_intr_md);
        transition parse_ethernet;
     }

     state parse_ethernet {
         packet.extract(hdr.ethernet);
         transition select (hdr.ethernet.ether_type) {
             ETHERTYPE_EFCP: parse_efcp;
             ETHERTYPE_VLAN: parse_vlan;
             ETHERTYPE_IPV4 : parse_ipv4;
             default : reject;
         }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.ether_type) {
            ETHERTYPE_EFCP: parse_efcp;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        // efcp_checksum.add(hdr.efcp);
        // eg_md.checksum_err_efcp_egprs = efcp_checksum.verify();
        // NOTE: we can remove it in the egress if we assume our modifications are fine?
        transition accept;
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        //ipv4_checksum.add(hdr.ipv4);
        //eg_md.checksum_err_ipv4_egprs = ipv4_checksum.verify();
        transition accept;
    }
}


/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   ********************
*************************************************************************/

control SwitchEgress(
        inout header_t hdr,
        inout egress_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_md_for_oport) {

    action mark_ecn() {
        hdr.efcp.flags = hdr.efcp.flags | 0x01;
    }

    apply {
       /* if (eg_md.enq_qdepth >= ECN_THRESHOLD) {
            mark_ecn();
        }

        if (hdr.vlan.vlan_id == 0) {
            hdr.vlan.setInvalid();
        }*/
    }

}


/****************************************************************************************
***********************  E G R E S S     D E P A R S E R  *******************************
****************************************************************************************/

control SwitchEgressDeparser(
        packet_out packet,
        inout header_t hdr,
        in egress_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {

    Checksum() efcp_checksum;
    Checksum() ipv4_checksum;

    apply {
        // Updating and checking of the checksum is done in the deparser.
        // Checksumming units are only available in the parser sections of
        // the program.
	 if (eg_md.checksum_pdu_efcp) {
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
                hdr.efcp.seqnum
            });
        }
        if (eg_md.checksum_pdu_ipv4) {
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
                 hdr.ipv4.dst_addr
            });
        }
	packet.emit(hdr);
	//packet.emit(hdr.ethernet);
        //packet.emit(hdr.efcp);
        //packet.emit(hdr.ipv4);
        //packet.emit(hdr.vlan);
    }
}



Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(), 
	 SwitchEgressDeparser()) pipe;

Switch(pipe) main;
