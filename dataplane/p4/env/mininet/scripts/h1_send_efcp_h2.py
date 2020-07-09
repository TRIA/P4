#!/usr/bin/env python

import argparse
import binascii
import struct
import pdu_types
import random
import re
import readline
import socket
import sys
from scapy.all import Packet, XByteField, ShortField, XShortField, IntField,\
        bind_layers, Ether, get_if_hwaddr, sendp, checksum, hexdump
from time import sleep

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    # ShortField --> 2 Byte-integer
    # IntEnumField --> 4 Byte-integer

    # EFCP PDU
    #   bytes 1-1 [0:0] = version
    #   bytes 2-3 [1:2] = dst_addr
    #   bytes 4-5 [3:4] = src_addr
    #   bytes 6-6 [5:5] = qos_id
    #   bytes 7-8 [6:7] = dst_cepid
    #   bytes 9-10 [8:9] = src_cepid
    #   bytes 11-11 [10:10] = pdu_type
    #   bytes 12-12 [11:11] = flags
    #   bytes 13-14 [12:13] = length
    #   bytes 15-18 [14:17] = seqnum
    #   bytes 19-20 [18:19] = chksum
    fields_desc = [ XByteField("version", 0x01),
                    ShortField("dst_addr", 0x00),
                    ShortField("src_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("dst_cepid", 0),
                    ShortField("src_cepid", 0),
                    XByteField("pdu_type", 0x00),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seqnum", 0),
                    XShortField("chksum", 0x00)]

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

bind_layers(Ether, EFCP, type=0xD1F)

def get_if(if_name = "eth0"):
    ifs = get_if_list()
    iface = None
    for i in get_if_list():
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
        pkt = Ether(src=get_if_hwaddr(iface), dst="00:00:00:00:00:02", type=0xD1F) / EFCP(dst_addr=2, pdu_type=pdu_types.DATA_TRANSFER)
        pkt = pkt / "Hello World!"
        #srp1(pkt, iface=iface, timeout=1, verbose=True)
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("Hex dump for EFCP:")
        hexdump(pkt["EFCP"])
    except Exception as error:
        print(error)

if __name__ == "__main__":
    main()
