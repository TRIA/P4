#!/usr/bin/env python3

#
# Model switch must be configured with controlplane_vlan.py for this test
# to pass.
#

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

class TestVLANICMP(unittest.TestCase):

    # Tests that a ping on a VLAN gets relayed to port A and B
    def test_basic_icmp_vlan(self):
        icmp_ping = Ether(dst=SWITCH_MAC, src=EDF9_REAL_MAC) \
            / Dot1Q(vlan=VLAN_ID) \
            / IP(dst=EDF10_VLAN_IP, src=EDF9_VLAN_IP) \
            / ICMP(type=8)
        tA = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        tB = AsyncSniffer(iface=PORT_B_VETH, count=2, timeout=2)
        tA.start()
        tB.start()
        time.sleep(1)
        sendp(icmp_ping, count=1, iface=PORT_A_VETH, verbose=False)
        tA.join()
        tB.join()
        pktsA = tA.results
        pktsB = tB.results
        self.assertIsNotNone(pktsA)
        self.assertEqual(1, len(pktsA))
        self.assertIsNotNone(pktsB)
        self.assertEqual(1, len(pktsB))
        self.assertEqual(EDF10_REAL_MAC, pktsB[0].dst)
        self.assertEqual(EDF9_FAKE_MAC, pktsB[0].src)
        self.assertEqual(VLAN_ID, pktsB[0].vlan)

# Test EFCP packets embedded in Ethernet packet... This is VLAN
# switching so it shouldn't matter anyway. The check that we have
# an EFCP packet in and out though.
# class TestVLANEFCP(unittest.TestCase):
#     def test_efcp_basic(self):
#         efcp_pkt = Ether(dst=EDF10_FAKE_MAC, src=EDF9_REAL_MAC) \
#             / Dot1Q(vlan=1) \
#             / EFCP(ipc_dst_addr=12, ipc_src_addr=13)
#         t = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
#         t.start()
#         time.sleep(1)
#         sendp(efcp_pkt, count=1, iface=PORT_A_VETH, verbose=False)
#         t.join()
#         pkts = t.results
#         self.assertIsNotNone(pkts)
#         self.assertEqual(12, pkts[0].ipc_dst_addr)
#         self.assertEqual(13, pkts[0].ipc_src_addr)

if __name__ == '__main__':
    unittest.main()

