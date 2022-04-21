#!/usr/bin/env python3

import grpc
import os
import sys
from controlplane_l2 import L2ControlPlane
from controlplane_l3_ip import L3IPControlPlane
from base import ControlPlane, ControlPlaneMain
from cfg import *
from time import sleep

class IPRINAControlPlane(L3IPControlPlane):
    def __init__(self):
        self.name = "L3-IP-RINA"
        self.setup_ops = [
            ("Fake MACS translation", self.setupFakeMacs),
            ("L2 switching to ports", self.setupL2Forwarding),
            ("L3 IP routing", self.setupL3IPRouting),
            ("L3 RINA Routing", self.setupL3RinaRouting)
        ]

    def setupL3RinaRouting(self):
        mappings = [(EDF9_RINA_ADDR, EDF9_FAKE_MAC),
                    (EDF10_RINA_ADDR, EDF10_FAKE_MAC)]

        for i in mappings:
            te = self.p4helper.buildTableEntry(
                table_name="LocalIngress.rina_to_dmac.map",
                match_fields={
                    "efcp.dst_addr": i[0]
                },
                action_name="LocalIngress.rina_to_dmac.forward",
                action_params={
                    "dmac": i[1]
                })
            print("Forwarding RINA address " + str(i[0]) + " to MAC " + i[1])
            self.conn.WriteTableEntry(te)

if __name__ == "__main__":
    ControlPlaneMain(IPRINAControlPlane)
