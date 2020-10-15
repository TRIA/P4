#!/usr/bin/env python

import argparse
import sys
import socket
import random
import struct
import re
import pdu_types
import binascii

from scapy.all import Packet, XByteField, ShortField, XShortField, IntField,\
        bind_layers, Ether, get_if_hwaddr, get_if_list, sendp, checksum, \
        hexdump

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    #ShortField --> 2 Byte-integer
    #IntEnumField --> 4 Byte-integer

    fields_desc = [ XByteField("version", 0x01),
                    ShortField("dst_addr", 0x00),
                    ShortField("scr_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("dst_cepid", 0),
                    ShortField("src_cepid", 0),
                    XByteField("pdu_type", 0x00),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seqnum", 0),
                    XShortField("chksum", 0x00)]

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
        print "checksum -> " + str(ck)
        p = p[:18] + chr(ck>>8) + chr(ck&0xff) + p[20:]
        return p + pay

#    def pre_dissect(self, s):
#        length = s[12:14]
#        # if self.length != 1
#        if length != b"\x00\x01":
#            self.length = int.from_bytes(length, "big")
#       return s

    def mysummary(self):
        s = self.sprintf("%EFCP.src_addr% > %EFCP.dst_addr% %EFCP.pdu_type%")
        return s

bind_layers(Ether, EFCP, type=0xD1F)

def get_if(if_name = "eth0"):
    ifs = get_if_list()
    iface = None
    for i in ifs:
        if if_name in i:
            iface = i
            break;
    if not iface:
        print("Cannot find %s interface" % str(if_name))
        exit(1)
    return iface

def main():
    iface = get_if()
    try:
        pkt = Ether(src="00:00:00:00:01:01", dst="00:04:00:00:00:00", type=0xD1F) / EFCP(dst_addr=11, pdu_type=pdu_types.LAYER_MANAGEMENT)
        #srp1(pkt, iface=iface, timeout=1, verbose=True)
        sendp(pkt, iface=iface, verbose=True)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("Hex dump for EFCP:")
        hexdump(pkt["EFCP"])
    except Exception as error:
        print(error)

if __name__ == "__main__":
    main()
