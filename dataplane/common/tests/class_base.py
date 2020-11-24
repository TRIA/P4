from scapy.all import *

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    # ShortField --> 2 Byte-integer
    # IntEnumField --> 4 Byte-integer

    # EFCP PDU
    #   bytes 1-1 [0:0] = version
    #   bytes 2-3 [1:2] = ipc_dst_addr
    #   bytes 4-5 [3:4] = ipc_src_addr
    #   bytes 6-6 [5:5] = qos_id
    #   bytes 7-8 [6:7] = cep_dst_id
    #   bytes 9-10 [8:9] = cep_src_id
    #   bytes 11-11 [10:10] = pdu_type
    #   bytes 12-12 [11:11] = flags
    #   bytes 13-14 [12:13] = length
    #   bytes 15-18 [14:17] = seq_num
    #   bytes 19-20 [18:19] = hdr_chksum
    fields_desc = [ XByteField("version", 0x01),
                    ShortField("ipc_dst_addr", 0x00),
                    ShortField("ipc_src_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("cep_dst_id", 0),
                    ShortField("cep_src_id", 0),
                    ShortField("pdu_type", 0x0000),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seq_num", 0),
                    XShortField("hdr_checksum", 0x00)]


    def post_build(self, p, pay):
        # Re-compute checksum every time
        ck = checksum(p)
        p = p[:18] + chr(ck>>8) + chr(ck&0xff) + p[20:]
        return p + pay

#    def pre_dissect(self, s):
#        length = s[12:14]
#        if length != b"\x00\x01":
#            self.length = int.from_bytes(length, "big")
#        return s

    def mysummary(self):
        s = self.sprintf("%EFCP.src_addr% > %EFCP.dst_addr% %EFCP.pdu_type%")
        return s
