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
from scapy.all import Ether, Dot1Q, IP, get_if_hwaddr, bind_layers, get_if_list, sendp, checksum, hexdump
from time import sleep

# ScaPy initialisation
## IPv4 (untagged and tagged)
bind_layers(Ether, IP, type=0x0800)
bind_layers(Ether, Dot1Q, type=0x8100)
bind_layers(Dot1Q, IP, type=0x0800)


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

def get_packet_dot1q(type_content):
    return Dot1Q(prio=0, id=0, vlan=random.randint(0, 4095), type=type_content)

def main():
    iface = get_if()
    try:
        # First packet Ether / IP
        pkt = Ether(src=get_if_hwaddr(iface), dst="00:00:00:00:00:01", type=0x0800) / IP(dst="10.0.0.1")
        pkt = pkt / "Hello World!"
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("Hex dump for IP:")
        hexdump(pkt["IP"])

        # Second packet Ether / Dot1q /IP
        dot1q_pkt = get_packet_dot1q(0x0800)
        eth_pkt = Ether(src=get_if_hwaddr(iface), dst="00:00:00:00:00:01", type=0x8100)
        ip_pkt = IP(dst="10.0.0.1")
        pkt = eth_pkt / dot1q_pkt / ip_pkt
        pkt = pkt / "Hello World!"
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("- Hex dump for Dot1Q:")
        hexdump(pkt["Dot1Q"])
        print("Hex dump for IP:")
        hexdump(pkt["IP"])


    except Exception as error:
        print(error)

if __name__ == "__main__":
    main()
