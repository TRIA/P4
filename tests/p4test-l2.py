#!/usr/bin/env python3

import time
import unittest
import struct
import base
from EFCP import *
from scapy.layers.l2 import Dot1Q, Ether
from scapy.layers.inet import IP, ICMP
from scapy.sendrecv import sendp, AsyncSniffer
from scapy.fields import IntField, ShortField, XByteField, XShortField, XShortEnumField
from scapy.packet import Packet
from cfg import *

#
# Model switch must be configured with controlplane_l2.py for this test
# to pass.
#

# Test that a simple ICMP packet will be forwarded to the right
# MAC. This is a ping from EDF9 to EDF10.
class TestICMP(unittest.TestCase):
    def test_basic_icmp(self):
        # Ping
        icmp_ping = Ether(dst=EDF10_FAKE_MAC, src=EDF9_REAL_MAC) \
            / IP(dst=EDF10_IP, src=EDF9_IP) \
            / ICMP(type=8)
        t = AsyncSniffer(iface=PORT_B_VETH, count=1, timeout=2)
        t.start()
        time.sleep(1)
        sendp(icmp_ping, count=1, iface=PORT_A_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts)
        self.assertEqual(EDF10_REAL_MAC, pkts[0].dst)
        self.assertEqual(EDF9_FAKE_MAC, pkts[0].src)

        # Ping reply
        icmp_reply = Ether(dst=EDF9_FAKE_MAC, src=EDF10_REAL_MAC) \
            / IP(dst=EDF9_IP, src=EDF10_IP) \
            / ICMP(type=0)
        t = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        t.start()X
        time.sleep(1)
        sendp(icmp_reply, count=1, iface=PORT_B_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts)
        self.assertEqual(EDF9_REAL_MAC, pkts[0].dst)
        self.assertEqual(EDF10_FAKE_MAC, pkts[0].src)

    def test_broadcast(self):
        icmp_bcast = Ether(dst="FF:FF:FF:FF:FF:FF", src=EDF9_REAL_MAC)
        t = AsyncSniffer(iface=PORT_B_VETH, count=1, timeout=2)
        t.start()
        time.sleep(1)
        sendp(icmp_bcast, count=1, iface=PORT_A_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts)
        self.assertTrue(len(pkts) == 1)
        self.assertEqual("ff:ff:ff:ff:ff:ff", pkts[0].dst)

# Test EFCP packets embedded in Ethernet packet... This is L2
# switching so it shouldn't matter anyway. The check that we have
# an EFCP packet in and out though.
class TestEFCP(unittest.TestCase):
    def test_efcp_basic(self):
        efcp_pkt = Ether(dst=EDF10_FAKE_MAC, src=EDF9_REAL_MAC) \
            / EFCP(ipc_dst_addr=12, ipc_src_addr=13)
        t = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        t.start()
        time.sleep(1)
        sendp(efcp_pkt, count=1, iface=PORT_A_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts)
        self.assertEqual(12, pkts[0].ipc_dst_addr)
        self.assertEqual(13, pkts[0].ipc_src_addr)

if __name__ == '__main__':
    unittest.main()
