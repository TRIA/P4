#!/usr/bin/env python3

import sys
import os
import time
import unittest
import struct
import base
from scapy.layers.l2 import Dot1Q, Ether
from scapy.layers.inet import IP, ICMP
from scapy.sendrecv import sendp, AsyncSniffer
from scapy.fields import IntField, ShortField, XByteField, XShortField, XShortEnumField
from scapy.packet import Packet
from cfg import *
from EFCP import EFCP

class TestL3RINA(unittest.TestCase):
    def test_efcp_basic(self):
        efcp_pkt = Ether(dst=EDF10_FAKE_MAC, src=EDF9_REAL_MAC) \
            / EFCP(ipc_dst_addr=EDF9_RINA_ADDR, ipc_src_addr=EDF10_RINA_ADDR)
        t = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        t.start()
        time.sleep(1)
        sendp(efcp_pkt, count=1, iface=PORT_A_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts)
        self.assertEqual(13, pkts[0].ipc_dst_addr)
        self.assertEqual(14, pkts[0].ipc_src_addr)

if __name__ == '__main__':
    unittest.main()

