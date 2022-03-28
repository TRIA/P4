#!/usr/bin/env python3

import time
import unittest
import struct
from scapy.layers.l2 import Dot1Q, Ether
from scapy.layers.inet import IP, ICMP
from scapy.sendrecv import sendp, AsyncSniffer
from scapy.fields import IntField, ShortField, XByteField, XShortField, XShortEnumField
from scapy.packet import Packet, bind_layers

DATA_TRANSFER = 0x80
LAYER_MANAGEMENT = 0x40
ACK_ONLY = 0xC1
NACK_ONLY = 0xC2
ACK_AND_FLOW_CONTROL = 0xC5
NACK_AND_FLOW_CONTROL = 0xC6
FLOW_CONTROL_ONLY = 0xC4
SELECTIVE_ACK = 0xC9
SELECTIVE_NACK = 0xCA
SELECTIVE_ACK_AND_FLOW_CONTROL = 0xCD
SELECTIVE_NACK_AND_FLOW_CONTROL = 0xCE
CONTROL_ACK = 0xC0
RENDEVOUS = 0xCF

# Generate the EDF_PDU_TYPES dictionary, similar to Ethernet's ETHER_TYPES
local_vars = locals()
local_vars = [ _ for _ in filter(lambda x: not x.startswith("__") and x != "local_vars", local_vars)]
EFCP_TYPES = {}
for local_var in local_vars:
    local_val = locals()[local_var]
    EFCP_TYPES[local_var] = local_val

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    #ShortField --> 2 Byte-integer
    #IntEnumField --> 4 Byte-integer

    # Fields:
    # [0,1) version
    # [1,3) ipc_dst_addr
    # [3,5) ipc_src_addr
    # [5,6) qos_id
    # [6,7) cep_dst_id
    # [7,8) cep_src_id
    # [8,10) pdu_type
    # [10,11) flags
    # [11,13) length
    # [13,17) seq_num
    # [17,19) hdr_checksum
    fields_desc = [ XByteField("version", 0x01),
                    ShortField("ipc_dst_addr", 0x00),
                    ShortField("ipc_src_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("cep_dst_id", 0),
                    ShortField("cep_src_id", 0),
                    XShortEnumField("pdu_type", 0x8001, EFCP_TYPES),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seq_num", 0),
                    XShortField("hdr_checksum", 0x00)
                ]

    def hashret(self):
        return struct.pack("H", self.type) + self.payload.hashret()

    def answers(self, other):
        if isinstance(other, EFCP):
            if self.type == other.type:
                return self.payload.answers(other.payload)
        return 0

    def mysummary(self):
        return self.sprintf("%EFCP.ipc_src_addr% > %EFCP.ipc_dst_addr% %EFCP.pdu_type%")

bind_layers(Ether, EFCP, type=0xD1F)
