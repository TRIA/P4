from scapy.all import checksum
from scapy.all import IntField, ShortField, XByteField, XShortField
from edf_pdu_types import EFCP_TYPES
import codecs

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

#    @classmethod
#    def dispatch_hook(cls, _pkt=None, *args, **kargs):
#        if _pkt and len(_pkt) >= 14:
#            if struct.unpack("!H", _pkt[12:14])[0] <= 1500:
#                return Dot3
#        return cls

    def post_build(self, p, pay):
    #        # This is making a difference !? - It reduces the packet to 19Bytes and that seems OK (!?)
    #        #p = hex(1)[2:] + p[2:]
    #        print "self.length = " + str(self.length)
    #        l = len(p)
    #        print "length headers -> " + str(l)
    #        if pay:
    #            l += len(pay)
    #        print "length total -> " + str(l)
    #        if self.length == 1:
    #            #p = p[:12]+struct.pack("!H", l)+p[14:]
    #            p = p[:12] + hex(l)[2:] + p[14:]
    #            #p = p[:12] + str(l) + p[14:]
    #        if self.chksum is None:
    #            ck = checksum(p)
    #            print "checksum -> " + str(ck)
    #            p = p[:18]+chr(ck>>8)+chr(ck&0xff)+p[20:]
        # Compute checksum every time!
        ck = checksum(p)
        print("checksum -> " + str(ck))
        #p = p[:18] + chr(ck>>8) + chr(ck&0xff) + p[20:]
        p = p[:18] + codecs.encode(bytes(chr(ck>>8), encoding="utf8"), "hex") + codecs.encode(bytes(chr(ck&0xff), encoding="utf8"), "hex") + p[20:]
        return p + pay


class VLAN(Packet):
    name = "VLAN"
    fields_desc = [ShortField("pcp_dei_id", 0x00), ShortField("ethertype", 0xD1F)]
