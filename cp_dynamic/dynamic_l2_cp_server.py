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

class MacToPortEntry:
    def __init__(self, mac, port, entry):
        self.mac = mac
        self.port = port
        self.entry = entry

class MacSpoofEntry:
    def __init__(self, mac, spoofed_mac, entry):
        self.mac = mac
        self.spoofed_mac = spoofed_mac
        self.entry = entry

class L2RouterControlServicer(L2RouterControlServicer):
    def __init__(self, p4helper, conn):
        self.p4helper = p4helper
        self.conn = conn

        # Save the table entries in a map so that we can remove them
        # without having to browse the table.
        self._macToPort = {}
        self._spoofDmac = {}
        self._spoofSmac = {}

        # Multicast port management
        self._ports = set()
        self._entryDone = False

    def GetStatus(self, r, context):
        return StatusReply(isOk=True)

    def _PortMulticastEntries(self):
        mg = []
        for p in self._ports:
            mg.append({
                "egress_port": p,
                "instance": 1
            })
        return mg

    def AddPort(self, r, context):
        if r.port in self._ports:
            print("Trying to register already existing port %d, it's okay!" % r.port)
            return StatusReply(isOk=True)
        try:
            mge = self.p4helper.buildMulticastGroupEntry(1, self._PortMulticastEntries())
            if self._entryDone:
                self.conn.ModifyPREEntry(mge)
            else:
                self.conn.WritePREEntry(mge)
            print("Added port %d to switch control plane" % r.port)
            self._ports.add(r.port)
            self._entryDone = True
            return StatusReply(isOk=True)
        except Exception as e:
            print("Failed to add port %d: %s" % (str(r.port), str(e)))
            return StatusReply(isOk=False, message=str(e))

    def DelPort(self, r, context):
        if not r.port in self._ports:
            print("Trying to unregister non-registered port %d, it's okay!" % r.port)
            return StatusReply(isOk=True)
        try:
            self._ports.remove(r.port)
            mge = self.p4helper.buildMulticastGroupEntry(1, self._PortMulticastEntries())
            self.conn.ModifyPREEntry(mge)
            print("Removed port %d from switch control plane" % r.port)
            return StatusReply(isOk=True)
        except Exception as e:
            print("Failed to remove port %d: %s" % (r.port, str(e)))
            return StatusReply(isOk=False, message=str(e))

    def AddMACToPort(self, r, context):
        if not r.port in self._ports:
            print("Trying to use an unregistered port %d for MAC %s" % (r.port, r.mac))
            return StatusReply(isOk=False, message=("No such port registered: %d" % r.port))
        try:
            if r.mac in self._macToPort and self._macToPort[r.mac].port == r.port:
                print("Entry for %s to port %d already exists, it's okay!" % (r.mac, r.port))
                return StatusReply(isOk=True)
            else:
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
                print("Forwarding destination MAC %s on port %d" % (r.mac, r.port))
                self._macToPort[r.mac] = MacToPortEntry(r.mac, r.port, tdest);
                return StatusReply(isOk=True)
        except Exception as e:
            print("Failed forward of destination MAC %s on port %d: %s" % (r.mac, r.port, str(e)))
            return StatusReply(isOk=False, message=str(e))

    def DelMACToPort(self, r, context):
        if not r.mac in self._macToPort:
            return StatusReply(isOk=True)
        try:
            self.conn.DeleteTableEntry(self._macToPort[r.mac].entry)
            del self._macToPort[r.mac]
            print("Removed forwarding for destination MAC %s" % r.mac)
            return StatusReply(isOk=True)
        except Exception as e:
            print("Error removing forwarding for destination MAC %s: %s" % (r.mac, str(e)))
            return StatusReply(isOk=False, message=str(e))

    def _AddSpoofedSMAC(self, mac, spoofed_mac):
        """This changes the SMAC of packets from 'spoofed_mac' to 'mac'"""
        if mac in self._spoofSmac:
            print("Already a spoofed source MAC %s, it's okay!" % mac)
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
        print("Spoofing source MAC %s to %s" % (mac, spoofed_mac))

    def _AddSpoofedDMAC(self, mac, spoofed_mac):
        """This changes the DMAC of packets destined for 'mac' to 'spoof_mac'"""
        if mac in self._spoofDmac:
            print("Already a spoofed destination MAC for %s, it's okay!" % mac)
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
        print("Spoofing destination MAC %s to %s" % (spoofed_mac, mac))

    def _DelSpoofedDMAC(self, mac):
        if not mac in self._spoofDmac:
            return
        self.conn.DeleteTableEntry(self._spoofDmac[mac])
        del self._spoofDmac[mac]
        print("Removed spoofed destination MAC %s" % mac)

    def _DelSpoofedSMAC(self, mac):
        if not mac in self._spoofSmac:
            return
        self.conn.DeleteTableEntry(self._spoofSmac[mac])
        del self._spoofSmac[mac]
        print("Removed spoofed source MAC %s" % mac)

    def AddSpoofedMAC(self, r, ctx):
        try:
            self._AddSpoofedSMAC(r.mac, r.spoofed_mac)
        except Exception as e:
            print("Failed to spoof source MAC %s to %s" % (r.mac, r.spoofed_mac))
            return StatusReply(isOk=False, message=str(e))
        try:
            self._AddSpoofedDMAC(r.mac, r.spoofed_mac)
        except Exception as e:
            print("Failed to spoof destination MAC %s to %s: %s" % (r.mac, r.spoofed_mac, str(e)))
            self._DelSpoofedSMAC(r.mac)
            return StatusReply(isOk=False, message=str(e))
        return StatusReply(isOk=True)

    def DelSpoofedMAC(self, r, context):
        try:
            self._DelSpoofedSMAC(r.mac)
            self._DelSpoofedDMAC(r.mac)
            return StatusReply(isOk=True)
        except Exception as e:
            print("Failed to remove spoofed MAC %s: %s" % (r.mac, str(e)))
            return StatusReply(isOk=False, message=str(e))

class L2DynamicControlPlane(ControlPlane):
    def __init__(self):
        self.name = "L2-Dynamic"
        self.setup_ops = [
            ("GRPC Server", self.startGrpcServer),
        ]

    def startGrpcServer(self):
        self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
        servicer = L2RouterControlServicer(self.p4helper, self.conn)
        add_L2RouterControlServicer_to_server(servicer, self.server)
        self.server.add_insecure_port('[::]:5051')
        self.server.start()

if __name__ == '__main__':
    ControlPlaneMain(L2DynamicControlPlane)
