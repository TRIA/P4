#!/usr/bin/env python3

import grpc
import os
import sys
from controlplane_l2 import L2ControlPlane
from base import ControlPlane, ControlPlaneMain
from cfg import *
from time import sleep

class VlanControlPlane(L2ControlPlane):
    def __init__(self):
        self.name = "vlan"
        self.setup_ops = [
            ("Fake MACs translation", self.setupFakeMacs),
            ("VLAN IDs to multicast groups", self.setupVlanIds),
            ("Multicast groups to egress ports", self.setupMcastGroups),
            ("Egress port destination MAC addresses", self.setupPortToMac)
        ]

    def setupVlanIds(self):
        # VLAN to multicast-group table
        te = self.p4helper.buildTableEntry(
            table_name="SwitchIngress.vlan_map",
            match_fields={
                "hdr.dot1q.vlan_id": VLAN_ID
            },
            action_name="SwitchIngress.vlan_forward",
            action_params={
                "mcast_gid": 1
            })
        self.conn.WriteTableEntry(te)

    def setupMcastGroups(self):
        # Multicast groupI ha for VLAN
        ge = self.p4helper.buildMulticastGroupEntry(1, [{
            "egress_port": PORT_A_NO,
            "instance": 1
        },{
            "egress_port": PORT_B_NO,
            "instance": 1
        }])
        self.conn.WritePREEntry(ge)

    def setupPortToMac(self):
        for p in [(PORT_A_NO, EDF9_REAL_MAC),
                  (PORT_B_NO, EDF10_REAL_MAC)]:
            te = self.p4helper.buildTableEntry(
                table_name="SwitchEgress.port_to_dmac.map",
                match_fields={
                    "dport": p[0]
                },
                action_name="SwitchEgress.port_to_dmac.set_dmac",
                action_params={
                    "dmac": p[1]
                })
            print("Port %d sets DMAC %s" % (p[0], p[1]))
            self.conn.WriteTableEntry(te)

if __name__ == "__main__":
    ControlPlaneMain(VlanControlPlane)
