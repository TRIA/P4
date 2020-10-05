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
from scapy.all import Ether, IP, get_if_hwaddr, get_if_list, sendp, checksum, hexdump
from time import sleep

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
        pkt = Ether(src=get_if_hwaddr(iface), dst="00:00:00:00:00:02", type=0x0800) / IP(dst="10.0.0.2")
        pkt = pkt / "Hello World!"
        sendp(pkt, iface=iface, verbose=False)
        pkt.show()
        print("Hex dump for whole packet:")
        hexdump(pkt)
        print("Hex dump for IP:")
        hexdump(pkt["IP"])
    except Exception as error:
        print(error)

if __name__ == "__main__":
    main()
