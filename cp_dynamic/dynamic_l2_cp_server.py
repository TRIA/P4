# This is a GRPC-based dynamic control plane for the L2 part of the
# P4 program.

from concurrent import futures
import sys
import logging
import grpc
from routerctl_pb2_grpc import L2RouterControlServicer, \
    add_L2RouterControlServicer_to_server
from status_pb2 import StatusReply

sys.path.append("cp_static")

from base import ControlPlane, ControlPlaneMain

class L2RouterControlServicer(L2RouterControlServicer):
    def __init__(self, p4helper, conn):
        self.p4helper = p4helper
        self.conn = conn

        # Save the table entries in a map so that we can remove them
        # without having to browse the table.
        self._macToPort = {}
        self._spoofDmac = {}
        self._spoofSmac = {}

    def AddMACToPort(self, r, context):
        try:
            tdest = self.p4helper.buildTableEntry(
                table_name="SwitchIngress.dmac_to_port.map",
                match_fields={
                    "eth.dst_addr": r.mac
                },
                action_name="SwitchIngress.dmac_to_port.set_dport",
                action_params={
                    "dport": r.port
                })
            self.conn.WriteTableEntry(tdest)
            print("Forwarding destination MAC " + r.mac + " on port " + str(r.port))
            self._macToPort[r.mac] = tdest;
            return StatusReply(isOk=True)
        except Exception as e:
            print("Failed forward of destination MAC " + r.mac
                  + " on port " + str(r.port)
                  + ": " + str(e))
            return StatusReply(isOk=False, message=str(e))

    def DelMACToPort(self, r, context):
        if not r.mac in self._macToPort:
            return StatusReply(isOk=True)
        try:
            self.conn.DeleteTableEntry(self._macToPort[r.mac])
            del self._macToPort[r.mac]
            print("Removed forwarding for destination MAC: " + r.mac)
            return StatusReply(isOk=True)
        except Exception as e:
            print("Error removing forwarding for destination MAC " + r.mac + ": " + str(e))
            return StatusReply(isOk=False, message=str(e))

    def _AddSpoofedSMAC(self, mac, spoofed_mac):
        """This changes the SMAC of packets from 'spoofed_mac' to 'mac'"""
        if mac in self._spoofSmac:
            print("Already a spoofed source MAC: " + mac)
            return
        ste = self.p4helper.buildTableEntry(
            table_name="SwitchEgress.smac.spoof_map",
            match_fields={
                "mac": mac
            },
            action_name="SwitchEgress.smac.spoof",
            action_params={
                "new_mac": spoofed_mac
            })
        self.conn.WriteTableEntry(ste)
        self._spoofSmac[mac] = ste
        print("Spoofing source MAC " + mac + " to " + spoofed_mac)

    def _AddSpoofedDMAC(self, mac, spoofed_mac):
        """This changes the DMAC of packets destined for 'mac' to 'spoof_mac'"""
        if mac in self._spoofDmac:
            print("Already a spoofed destination MAC: " + mac)
            return
        dte = self.p4helper.buildTableEntry(
            table_name="SwitchEgress.dmac.spoof_map",
            match_fields={
                "mac": spoofed_mac
            },
            action_name="SwitchEgress.dmac.spoof",
            action_params={
                "new_mac": mac
            })
        self.conn.WriteTableEntry(dte)
        self._spoofDmac[mac] = dte
        print("Spoofing destination MAC " + spoofed_mac + " to " + mac)

    def _DelSpoofedDMAC(self, mac):
        if not mac in self._spoofDmac:
            return
        self.conn.DeleteTableEntry(self._spoofDmac[mac])
        del self._spoofDmac[mac]
        print("Removed spoofed destination MAC " + mac)

    def _DelSpoofedSMAC(self, mac):
        if not mac in self._spoofSmac:
            return
        self.conn.DeleteTableEntry(self._spoofSmac[mac])
        del self._spoofSmac[mac]
        print("Removed spoofed source MAC " + mac)

    def AddSpoofedMAC(self, r, ctx):
        try:
            self._AddSpoofedSMAC(r.mac, r.spoofed_mac)
        except Exception as e:
            print("Failed to spoof source MAC " + r.mac + " to " + r.spoofed_mac)
            return StatusReply(isOk=False, message=str(e))
        try:
            self._AddSpoofedDMAC(r.mac, r.spoofed_mac)
        except Exception as e:
            print("Failed to spoof destination MAC " + r.mac + " to " + r.spoofed_mac)
            self._DelSpoofedSMAC(r.mac, r.spoofed_mac)
            return StatusReply(isOk=False, message=str(e))
        return StatusReply(isOk=True)

    def DelSpoofedMAC(self, r, context):
        try:
            self._DelSpoofedSMAC(r.mac)
            self._DelSpoofedDMAC(r.mac)
            return StatusReply(isOk=True)
        except Exception as e:
            print("Failed to remove spoofed MAC " + r.mac + ": " + str(e))
            return StatusReply(isOk=False, message=str(e))

class L2DynamicControlPlane(ControlPlane):
    def __init__(self):
        self.name = "L2-Dynamic"
        self.setup_ops = [
            ("GRPC Server", self.startGrpcServer)
        ]

    def startGrpcServer(self):
        self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
        servicer = L2RouterControlServicer(self.p4helper, self.conn)
        add_L2RouterControlServicer_to_server(servicer, self.server)
        self.server.add_insecure_port('[::]:50051')
        self.server.start()

if __name__ == '__main__':
    ControlPlaneMain(L2DynamicControlPlane)
