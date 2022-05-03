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

#
# Model switch must be configured with controlplane_l3ip.py for this test
# to pass.
#

class TestL3ICMP(unittest.TestCase):

    # Test that a ICMP ping packet will get routed to the right port
    # by routing using the IP address. This tests packets are routed
    # using /32 routes.
    def test_basic_icmp1(self):
        # Ping. The target MAC is the address of the switch.
        icmp_ping = Ether(dst=SWITCH_MAC, src=EDF9_FAKE_MAC) \
            / IP(dst=EDF10_IP, src=EDF9_IP) \
            / ICMP(type=8)

        # Look at port B to see if the packet will get out to that
        # port.
        tB = AsyncSniffer(iface=PORT_B_VETH, count=1, timeout=2)
        tB.start()

        # Make sure nothing comes out of port A
        tA = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        tA.start()

        time.sleep(1)

        # Send to port A
        sendp(icmp_ping, count=1, iface=PORT_A_VETH, verbose=False)
        tB.join()
        tA.join()

        # Look at the output of the single packet on port 133
        pkts = tB.results
        self.assertIsNotNone(pkts, "No packet data received")
        self.assertTrue(len(pkts) > 0, ("Nb of received packets %d, expected more than one") % len(pkts))
        self.assertEqual(EDF10_REAL_MAC, pkts[0].dst, ("Expected ethernet dst %s, received %s") % (EDF10_REAL_MAC, pkts[0].dst))
        self.assertEqual(SWITCH_MAC, pkts[0].src, ("Expected ethernet src %s, received %s") % (SWITCH_MAC, pkts[0].src))

        # Make sure port 132 doesn't have an extra packet.
        pkts = tA.results
        self.assertTrue(len(pkts) == 1)

    def test_basic_icmp2(self):
        pkt1 = Ether(dst=SWITCH_MAC, src=EDF9_REAL_MAC) \
            / IP(dst=EDF_DOT3X_IP_1, src=EDF9_IP)
        pkt2 = Ether(dst=SWITCH_MAC, src=EDF9_REAL_MAC) \
            / IP(dst=EDF_DOT3X_IP_2, src=EDF9_IP)

        # Look at port 133, the packets should have been routed there.
        tB = AsyncSniffer(iface=PORT_B_VETH, count=2, timeout=2)
        tB.start()

        time.sleep(1)

        # Send 2 packets, at 2 different address, toward port A
        sendp(pkt1, count=1, iface=PORT_A_VETH, verbose=False)
        sendp(pkt2, count=1, iface=PORT_A_VETH, verbose=False)

        # They should both end up in port B
        tB.join()
        pkts = tB.results
        self.assertIsNotNone(pkts, "No packet data received")
        self.assertTrue(len(pkts) == 2, ("Nb of received packets %d, expected 2") % len(pkts))
        self.assertEqual(EDF_DOT3X_IP_1, pkts[0][IP].dst, ("Expected dest %s, received %s") % (EDF_DOT3X_IP_1, pkts[0][IP].dst))
        self.assertEqual(EDF_DOT3X_IP_2, pkts[1][IP].dst, ("Expected dest %s, received %s") % (EDF_DOT3X_IP_2, pkts[1][IP].dst))
        self.assertEqual(EDF9_IP, pkts[0][IP].src, ("Expected src %s, received %s") % (EDF9_IP, pkts[0][IP].src))
        self.assertEqual(EDF9_IP, pkts[1][IP].src, ("Expected src %s, received %s") % (EDF9_IP, pkts[1][IP].src))
        self.assertEqual(SWITCH_MAC, pkts[0].src, ("Excepted ethernet src %s, received %s") % (SWITCH_MAC, pkts[0].src))
        self.assertEqual(SWITCH_MAC, pkts[1].src, ("Excepted ethernet src %s, received %s") % (SWITCH_MAC, pkts[1].src))

    def test_bad_icmp_checksum(self):
        # Bad checksum means the packet should be discarded by the
        # switch.
        icmp_ping = Ether(dst=SWITCH_MAC, src=EDF9_REAL_MAC) \
            / IP(dst=EDF10_IP, src=EDF9_IP, chksum=0xBAD) \
            / ICMP(type=8)
        t = AsyncSniffer(iface=PORT_B_VETH, count=1, timeout=2)
        t.start()
        time.sleep(1)
        sendp(icmp_ping, count=1, iface=PORT_A_VETH, verbose=False)
        t.join()
        pkts = t.results
        self.assertIsNotNone(pkts, "No packets packets data received")
        self.assertTrue(len(pkts) == 0, ("Nb of received packets %d, expected 0") % len(pkts))

    def test_basic_icmp3(self):
        # Ping. The target MAC is the address of the switch.
        icmp_ping = Ether(dst=SWITCH_MAC, src=EDF9_REAL_MAC) \
            / IP(dst=EDF_DOT2X_IP_1, src=EDF9_IP) \
            / ICMP(type=8)

        # This one should be come back out of port 132
        tA = AsyncSniffer(iface=PORT_A_VETH, count=2, timeout=2)
        tA.start()

        time.sleep(1)

        # Send to port 132
        sendp(icmp_ping, count=1, iface=PORT_A_VETH, verbose=False)

        # Check the output back on port 132
        tA.join()

        pkts = tA.results
        self.assertIsNotNone(pkts, "No packets data received")
        self.assertTrue(len(pkts) == 2, ("Nb of received packets %d, expected 2") % len(pkts))

if __name__ == '__main__':
    unittest.main()
