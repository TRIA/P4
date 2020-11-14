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
from class_base import EFCP
from scapy.all import Packet, XByteField, ShortField, XShortField, IntField,\
        bind_layers, Ether, get_if_hwaddr, get_if_list, sendp, checksum, \
        hexdump
from time import sleep

# ScaPy initialisation
bind_layers(Ether, EFCP, type=0xD1F)
bind_layers(Ether, Dot1Q, type=0x8100)
bind_layers(Dot1Q, EFCP, type=0xD1F)

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

def get_packet_ether(type_content):
    return Ether(src=get_if_hwaddr(iface), dst="00:00:00:00:00:01", type=type_content)

def get_packet_efcp():
    efcp_pkt = EFCP(dst_addr=1, pdu_type=pdu_types.DATA_TRANSFER)
    efcp_payload = "EFCP packet #{} sent from CLI to BM :)".format(test_num)
    return efcp_pkt / efcp_payload

def get_packet_dot1q():
    return Dot1Q(prio=0, id=0, vlan=random(0, 4095), type=0xD1F)

def main():
    iface = get_if()
    try:
        efcp_pkt = get_packet_efcp()
        dot1q_pkt = get_packet_dot1q()

        # First packet: Ether / EFCP
        eth_pkt = get_packet_ether("0xD1F")
        pkt = eth_pkt / efcp_pkt
        #srp1(pkt, iface=iface, timeout=1, verbose=True)
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("- Hex dump for EFCP:")
        hexdump(pkt["EFCP"])
        
        # Second packet: Ether / Dot1Q / EFCP
        eth_pkt = get_packet_ether("0x8100")
        pkt = eth_pkt / dot1q_pkt / efcp_pkt
        #srp1(pkt, iface=iface, timeout=1, verbose=True)
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("- Hex dump for Dot1Q:")
        hexdump(pkt["Dot1Q"])
        print("- Hex dump for EFCP:")
        hexdump(pkt["EFCP"])
    except Exception as error:
        print(error)

if __name__ == "__main__":
    main()
