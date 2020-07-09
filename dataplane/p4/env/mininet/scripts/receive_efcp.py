#!/usr/bin/env python

import sys
import struct
import binascii
from scapy.all import get_if_list, sniff

def pkt_callback(pkt):
    # Debug statement
    pkt.show2()
    print("Raw header data: " + binascii.hexlify(str(pkt)))
    print("Computed checksum by the router is: " + binascii.hexlify(str(pkt))[-4:])

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

iface = get_if()
print("Sniffing on %s" % iface)
sys.stdout.flush()
sniff(filter="!ip6", iface=iface, prn=pkt_callback, store=0)
