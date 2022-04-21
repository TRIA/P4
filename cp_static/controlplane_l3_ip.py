#!/usr/bin/env python3

import grpc
import os
import sys
from controlplane_l2 import L2ControlPlane
from base import ControlPlane, ControlPlaneMain
from cfg import *
from time import sleep

class L3IPControlPlane(L2ControlPlane):
    def __init__(self):
        self.name = "L3-IP"
        self.setup_ops = [
            ("Fake MACS translation", self.setupFakeMacs),
            ("L2 switching to ports", self.setupL2Forwarding),
            ("L3 IP routing", self.setupL3IPRouting)
        ]

    def setupL3IPRouting(self):
        mappings = [([EDF9_IP, 32], EDF9_FAKE_MAC),
                    ([EDF10_IP, 32], EDF10_FAKE_MAC),
                    ([EDF9_RANGE, 24], EDF9_FAKE_MAC),
                    ([EDF10_RANGE, 24], EDF10_FAKE_MAC)]

        for i in mappings:
            te = self.p4helper.buildTableEntry(
                table_name="LocalIngress.ip_to_dmac.lpm",
                match_fields={
                    "ipv4.dst_addr": i[0]
                },
                action_name="LocalIngress.ip_to_dmac.forward",
                action_params={
                    "dmac": i[1],
                })
            print("Forwarding IP " + i[0][0] + "/" + str(i[0][1]) + " to MAC " + i[1])
            self.conn.WriteTableEntry(te)

if __name__ == "__main__":
    ControlPlaneMain(L3IPControlPlane)


