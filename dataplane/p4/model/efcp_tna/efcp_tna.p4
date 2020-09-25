/* -*- P4_16 -*- */
#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif
#include "../common/headers.p4"
#include "../common/util.p4"


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


/*
 * ECN threshold for congestion control
 */
const bit<19> ECN_THRESHOLD = 1;

/*
 * PDU Types
 */
const bit<8> DATA_TRANSFER = 0x80; //MAYBE THE NAME HAS TO BE: DATA_TRANSFER_T
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
typedef bit<128> srv6_sid_t;
struct srv6_metadata_t {
    srv6_sid_t sid; // SRH[SL]
    bit<16> rewrite; // Rewrite index
    bool psp; // Penultimate Segment Pop
    bool usp; // Ultimate Segment Pop
    bool decap;
    bool encap;
}*/

struct egress_metadata_t {
    bit<16> checksum_ipv4_tmp;
    bit<16> checksum_efcp_tmp;

    bool checksum_upd_ipv4;
    bool checksum_upd_efcp;

    bool checksum_err_ipv4_igprs;
    bool checksum_err_efcp_igprs;
 
    bit<19> enq_qdepth;
    //srv6_metadata_t srv6;
}


/*
 * All metadata, globally used in the program, also  needs to be assembed
 * into a single struct. As in the case of the headers, we only need to
 * declare the type, but there is no need to instantiate it,
 * because it is done "by the architecture", i.e. outside of P4 functions
 */

struct metadata_t {
    bit<16> checksum_ipv4_tmp;
    bit<16> checksum_efcp_tmp;

    bool checksum_upd_ipv4;
    bool checksum_upd_efcp;

    bool checksum_err_ipv4_igprs;
    bool checksum_err_efcp_igprs;
    bit<16> checksum_error;
    bit<16> parser_error;
    bit<9>  egress_spec;

}


/*
 * All headers, used in the program needs to be assembed into a single struct.
 * We only need to declare the type, but there is no need to instantiate it,
 * because it is done "by the architecture", i.e. outside of P4 functions. It is 
 * allocated in headers.p4
 */



// Declare user-defined errors that may be signaled during parsing
error {
    WrongPDUtype
}

/***************************************************************************************
*********************** I N G R E S S   P A R S E R  ***********************************
***************************************************************************************/
parser SwitchIngressParser(
        packet_in packet,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {
    //TofinoIngressParser() tofino_parser;
    Checksum() efcp_checksum;
    Checksum() ipv4_checksum;

    state start {
      //  tofino_parser.apply(packet, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            EFCP_ETYPE: parse_efcp;
            VLAN_ETYPE: parse_vlan;
            IPV4_ETYPE: parse_ipv4;
            default: accept;
        }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.etherType) {
            EFCP_ETYPE: parse_efcp;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        efcp_checksum.add(hdr.efcp);
        ig_md.checksum_err_efcp_igprs = efcp_checksum.verify();
        verify((hdr.efcp.pduType == DATA_TRANSFER ||
                hdr.efcp.pduType == LAYER_MANAGEMENT ||
                hdr.efcp.pduType == ACK_ONLY ||
                hdr.efcp.pduType == NACK_ONLY ||
                hdr.efcp.pduType == ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == FLOW_CONTROL_ONLY ||
                hdr.efcp.pduType == SELECTIVE_ACK ||
                hdr.efcp.pduType == SELECTIVE_NACK ||
                hdr.efcp.pduType == SELECTIVE_ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == SELECTIVE_NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == CONTROL_ACK ||
                hdr.efcp.pduType == RENDEVOUS)
	, error.WrongPDUtype);
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
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

Counter<bit<32>, PortId_t>(
        64, CounterType_t.PACKETS_AND_BYTES) ipv4_counter;

Counter<bit<32>, PortId_t>(
        64, CounterType_t.PACKETS_AND_BYTES) efcp_counter;


/*
 * EFCP forwarding action
 */
    action efcp_forward(bit<12> vlan_id, mac_addr_t dstAddr, egress_spec port) {
        hdr.vlan.vlan_id = vlan_id;
        ig_md.egress_spec = port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = dstAddr;
	efcp_counter.count(port);
    }
/*
 * Drop action
 */
    action miss() {
        ig_dprsr_md.drop_ctl = 0x1; // Drop packet.
    }

/*
 * IPv4 forwarding action
 */
    action ipv4_forward(mac_addr_t dstAddr, egress_spec port) {
        ig_md.egress_spec = port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = dstAddr;
        hdr.ipv4.ttl = hdr.ipv4.ttl - 1;
	ipv4_counter.count(port);
    }

/*
 * EFCP exact table
 */
    table efcp_lpm {
        key = {
            hdr.efcp.dstAddr: exact;
        }
        actions = {
            efcp_forward;
	    miss;
            NoAction;
        }
    //    size = 1024;
        default_action = NoAction();
    }

/*
 * IPv4 long prefix match table
 */
    table ipv4_lpm {
        key = {
            hdr.ipv4.dstAddr: lpm;
        }
        actions = {
            ipv4_forward;
	    miss;
            NoAction;
        }
      //  size = 1024; default_action = miss();
    }

    apply {
        if (hdr.efcp.isValid() &&
            ig_md.checksum_error == 0) {
                if (hdr.efcp.pduType == LAYER_MANAGEMENT) {
                    ig_md.egress_spec = CPU_PORT;
                } else {
                    efcp_lpm.apply();
                }
        } else if (hdr.ipv4.isValid() &&
            ig_md.checksum_error == 0) {
                ipv4_lpm.apply();
        } else {
            miss();
        }
    }
}


/******************************************************************************************
***********************  I N G R E S S     D E P A R S E R  *******************************
******************************************************************************************/

control SwitchIngressDeparser(packet_out packet,
                              inout header_t hdr,
                              in metadata_t ig_md,
                              in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    Checksum() efcp_checksum;
    Checksum() ipv4_checksum;
    apply {
        // Updating and checking of the checksum is done in the deparser.
        // Checksumming units are only available in the parser sections of
        // the program.
        if (ig_md.checksum_upd_ipv4) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update(
                {hdr.ipv4.version,
                 hdr.ipv4.ihl,
                 hdr.ipv4.diffserv,
                 hdr.ipv4.total_len,
                 hdr.ipv4.identification,
                 hdr.ipv4.flags,
                 hdr.ipv4.frag_offset,
                 hdr.ipv4.ttl,
                 hdr.ipv4.protocol,
                 hdr.ipv4.srcAddr,
                 hdr.ipv4.dstAddr});
        }
	 if (ig_md.checksum_upd_efcp) {
            hdr.efcp.hdr_checksum = efcp_checksum.update(
                {hdr.efcp.ver,
	      hdr.efcp.dstAddr,
              hdr.efcp.srcAddr,
              hdr.efcp.qosID,
              hdr.efcp.dstCEPID,
              hdr.efcp.srcCEPID,
              hdr.efcp.pduType,
              hdr.efcp.flags,
              hdr.efcp.len,
              hdr.efcp.seqnum});
        }
	packet.emit(hdr.ethernet);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.vlan);

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

    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            EFCP_ETYPE: parse_efcp; //YO CREO QUE SI HACE FALTA VALIDAR CHECKSUM
            VLAN_ETYPE: parse_vlan;
            IPV4_ETYPE: parse_ipv4;
            default: accept;
        }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.etherType) {
            EFCP_ETYPE: parse_efcp;
            default: accept;
        }
    }

    state parse_efcp {
        packet.extract(hdr.efcp);
        verify((hdr.efcp.pduType == DATA_TRANSFER ||
                hdr.efcp.pduType == LAYER_MANAGEMENT ||
                hdr.efcp.pduType == ACK_ONLY ||
                hdr.efcp.pduType == NACK_ONLY ||
                hdr.efcp.pduType == ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == FLOW_CONTROL_ONLY ||
                hdr.efcp.pduType == SELECTIVE_ACK ||
                hdr.efcp.pduType == SELECTIVE_NACK ||
                hdr.efcp.pduType == SELECTIVE_ACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == SELECTIVE_NACK_AND_FLOW_CONTROL ||
                hdr.efcp.pduType == CONTROL_ACK ||
                hdr.efcp.pduType == RENDEVOUS)
            , error.WrongPDUtype);
        transition accept;
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
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
        if (eg_md.enq_qdepth >= ECN_THRESHOLD) {
            mark_ecn();
        }

        if (hdr.vlan.vlan_id == 0) {
            hdr.vlan.setInvalid();
        }
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
        if (eg_md.checksum_upd_ipv4) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update(
                {hdr.ipv4.version,
                 hdr.ipv4.ihl,
                 hdr.ipv4.diffserv,
                 hdr.ipv4.total_len,
                 hdr.ipv4.identification,
                 hdr.ipv4.flags, //REVISAR QUE LOS NOMBRES SON ASÍ
                 hdr.ipv4.frag_offset,
                 hdr.ipv4.ttl,
                 hdr.ipv4.protocol,
                 hdr.ipv4.srcAddr,
                 hdr.ipv4.dstAddr});
        }
	 if (eg_md.checksum_upd_efcp) {
            hdr.efcp.hdr_checksum = efcp_checksum.update(
                {hdr.efcp.ver,
	      hdr.efcp.dstAddr,
              hdr.efcp.srcAddr,
              hdr.efcp.qosID,
              hdr.efcp.dstCEPID,
              hdr.efcp.srcCEPID,
              hdr.efcp.pduType,
              hdr.efcp.flags,
              hdr.efcp.len,
              hdr.efcp.seqnum});
        }
	packet.emit(hdr.ethernet);
        packet.emit(hdr.efcp);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.vlan);

    }    
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

Pipeline(SwitchIngressParser(), //DONE pero hay que revisar cosas
         SwitchIngress(), //
         SwitchIngressDeparser(), //DONE
         SwitchEgressParser(), //DONE pero hay que revisar cosas
         SwitchEgress(), //
         SwitchEgressDeparser()) pipe; //DONE pero hay que revisar cosas

Switch(pipe) main;
