#!/usr/bin/env python3

import grpc
import os
import sys
from base import ControlPlane, ControlPlaneMain
from cfg import *
from time import sleep

class L2ControlPlane(ControlPlane):
    def __init__(self):
        self.name = "L2"
        self.setup_ops = [
            ("L2 broadcast", self.setupBroadcasting),
            ("Fake MACs translation", self.setupFakeMacs),
            ("L2 switching to ports", self.setupL2Forwarding)
        ]

    def setupBroadcasting(self):
        # Multicast group for broadcasting.
        ge = self.p4helper.buildMulticastGroupEntry(1, [{
            "egress_port": PORT_A_NO,
            "instance": 1
        },{
            "egress_port": PORT_B_NO,
            "instance": 1
        }])
        self.conn.WritePREEntry(ge)

        # VLAN to multicast-group table
        te = self.p4helper.buildTableEntry(
            table_name="SwitchIngress.broadcast_map",
            match_fields={
                "hdr.ethernet.dst_addr": "ff:ff:ff:ff:ff:ff"
            },
            action_name="SwitchIngress.broadcast",
            action_params={
                "mcast_gid": 1
            })
        self.conn.WriteTableEntry(te)

    def setupL2Forwarding(self):
        # Destination forwarding
        for i in [(EDF9_FAKE_MAC, PORT_A_NO),
                  (EDF10_FAKE_MAC, PORT_B_NO)]:
            tdest=self.p4helper.buildTableEntry(
                table_name="SwitchIngress.dmac_to_port.map",
                match_fields={
                    "eth.dst_addr": i[0]
                },
                action_name="SwitchIngress.dmac_to_port.set_dport",
                action_params={
                    "dport": i[1]
                })
            print("Forwarding destination " + i[0] + " on port " + str(i[1]))
            self.conn.WriteTableEntry(tdest)

    def setupFakeMacs(self):
        # Fake MACS to real macs
        translations=[(EDF10_FAKE_MAC, EDF10_REAL_MAC),
                      (EDF9_FAKE_MAC, EDF9_REAL_MAC)]

        # Destination MAC translations
        for i in translations:
            te = self.p4helper.buildTableEntry(
                table_name="SwitchEgress.dmac.spoof_map",
                match_fields={
                    "mac": i[0]
                },
                action_name="SwitchEgress.dmac.spoof",
                action_params={
                    "new_mac": i[1]
                })
            print("Translating destination MAC "+ i[0] + " to " + i[1])
            self.conn.WriteTableEntry(te)

        # Source MAC translations
        for i in translations:
            te = self.p4helper.buildTableEntry(
                table_name="SwitchEgress.smac.spoof_map",
                match_fields={
                    "mac": i[1]
                },
                action_name="SwitchEgress.smac.spoof",
                action_params={
                    "new_mac": i[0]
                })
            print("Translating source MAC " + i[1] + " to " + i[0])
            self.conn.WriteTableEntry(te)

if __name__ == "__main__":
    ControlPlaneMain(L2ControlPlane)
