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
            ("Fake MACs translation", self.setupFakeMacs),
            ("L2 switching to ports", self.setupL2Forwarding)
        ]

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
                table_name="SwitchEgress.dmac.changeTo",
                match_fields={
                    "mac": i[0]
                },
                action_name="SwitchEgress.dmac.translate",
                action_params={
                    "new_mac": i[1]
                })
            print("Translating destination MAC "+ i[0] + " to " + i[1])
            self.conn.WriteTableEntry(te)

        # Source MAC translations
        for i in translations:
            te = self.p4helper.buildTableEntry(
                table_name="SwitchEgress.smac.changeTo",
                match_fields={
                    "mac": i[1]
                },
                action_name="SwitchEgress.smac.translate",
                action_params={
                    "new_mac": i[0]
                })
            print("Translating source MAC " + i[1] + " to " + i[0])
            self.conn.WriteTableEntry(te)

if __name__ == "__main__":
    ControlPlaneMain(L2ControlPlane)
