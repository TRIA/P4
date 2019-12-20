from scapy.all import *

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    #ShortField --> 2 Byte-integer
    #IntEnumField --> 4 Byte-integer

    fields_desc = [ XByteField("version", 0x01),
                    ShortField("ipc_dst_addr", 0x00),
                    ShortField("ipc_src_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("cep_dst_id", 0),
                    ShortField("cep_src_id", 0),
                    ShortField("pdutype", 0x0000),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seq_num", 0),
                    XShortField("hdr_checksum", 0x00)]

class VLAN(Packet):
    name = "VLAN"
    fields_desc = [ShortField("pcp_dei_id", 0x00), ShortField("ethertype", 0xD1F)]
