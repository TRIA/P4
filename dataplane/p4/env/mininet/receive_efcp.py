#!/usr/bin/env python

import sys
import struct
import binascii

from common import get_if
from scapy.all import *

def pkt_callback(pkt):
    # Debug statement
    pkt.show2()
    print "Raw header data: " + binascii.hexlify(str(pkt))
    print "Computed checksum by the router is: " + binascii.hexlify(str(pkt))[-4:]

iface = get_if()
print "Sniffing on %s" % iface
sys.stdout.flush()
sniff(filter="!ip6", iface=iface, prn=pkt_callback, store=0)
