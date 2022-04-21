import sys
import os
import time

this_file = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(this_file, '../config'))
sys.path.append(os.path.join(this_file, '../common/python/utils'))

import p4runtime_lib.tofino
from p4runtime_lib.switch import ShutdownAllSwitchConnections
import p4runtime_lib.helper

from cfg import *

def printGrpcError(e):
    print("gRPC Error:", e.details())
    status_code=e.code()
    print("(%s)" % status_code.name,)
    traceback=sys.exc_info()[2]
    print("[%s:%d]" % (traceback.tb_frame.f_code.co_filename, traceback.tb_lineno))

class ControlPlane(object):
    def connect(self, address, port, dev_id, p4info_file_path, tofino_bin_path):
        self._address = address
        self._port = port
        self._dev_id = dev_id
        self._p4info_file_path = p4info_file_path
        self._tofino_bin_path = tofino_bin_path

        self.p4helper = p4runtime_lib.helper.P4InfoHelper(self._p4info_file_path)
        self.conn = p4runtime_lib.tofino.TofinoSwitchConnection(
            name=self.name,
            address=address + ":" + port,
            device_id=int(dev_id),
            proto_dump_file='logs/%s-runtime.txt' % self.name)

    def setup(self):
        self.conn.MasterArbitrationUpdate()
        self.conn.SetForwardingPipelineConfig(p4info=self.p4helper.p4info,
                                              tofino_bin_path=self._tofino_bin_path)

        print("START switch setup, %d operation(s)" % len(self.setup_ops))
        for op in self.setup_ops:
            print("Setting up: %s" % op[0])
            op[1]()
        print("END of switch setup")

    def _getCounterData(self, ids, sw, index):
        for response in sw.ReadCounters(ids, index):
            entity = response.entities[0]
            return entity.counter_entry.data

    def getFwdDropCounter(self, p4helper, sw, counter_name):
        nb_fwd = 0
        nb_drop = 0
        ids = p4helper.get_counters_id(counter_name)
        nb_fwd = self._getCounterData(ids, sw, 0).packet_count
        nb_drop = self._getCounterData(ids, sw, 1).packet_count
        return ("FWD: %d DROP: %d" % (nb_fwd, nb_drop))

    def formatIngressCounters(self, p4helper, sw, eth_cnt_name, type_cnt_name):
        eth_ids = p4helper.get_counters_id(eth_cnt_name)
        nb_eth = self._getCounterData(eth_ids, sw, 0) # Ethernet packets
        type_ids = p4helper.get_counters_id(type_cnt_name)
        nb_ipv4   = self._getCounterData(type_ids, sw, 0) # CNT_IN_IPV4
        nb_ipv6   = self._getCounterData(type_ids, sw, 1) # CNT_IN_IPV6
        nb_efcp   = self._getCounterData(type_ids, sw, 2) # CNT_IN_EFCP
        nb_arp    = self._getCounterData(type_ids, sw, 3) # CNT_IN_ARP
        nb_vlan   = self._getCounterData(type_ids, sw, 4) # CNT_IN_VLAN
        nb_rinarp = self._getCounterData(type_ids, sw, 5) # CNT_IN_RINARP
        nb_oth    = self._getCounterData(type_ids, sw, 6) # CNT_IN_OTHER
        return ("ETH: %d IPV4: %d IPV6: %d EFCP: %d ARP: %d VLAN: %d RINARP: %d OTH: %d" % \
                (nb_eth.packet_count, nb_ipv4.packet_count, nb_ipv6.packet_count,
                 nb_efcp.packet_count, nb_arp.packet_count, nb_vlan.packet_count,
                 nb_rinarp.packet_count, nb_oth.packet_count))

    def waitLoop(self):
        while True:
            try:
                print("----------------------------------------------------------------------")
                print("IP Packets\t " \
                      + self.getFwdDropCounter(self.p4helper, self.conn, "LocalIngress.ip_to_dmac.cnt"))
                print("RINA Packets\t " \
                      + self.getFwdDropCounter(self.p4helper, self.conn, "LocalIngress.rina_to_dmac.cnt"))
                print(self.formatIngressCounters(self.p4helper, self.conn,
                                                 "LocalIngress.ingress_cnt.eth",
                                                 "LocalIngress.ingress_cnt.per_type"))
                print("----------------------------------------------------------------------")

            except Exception as e:
                print("Error reading counters: ", e)
            time.sleep(10)

def ControlPlaneMain(control_plane):
    if len(sys.argv) < 3:
        print("Usage: ./p4runtime.py <p4 output> <p4info output> <address> <port> <device id>")
        sys.exit(1)
    else:
        p4_output = sys.argv[1]
        p4info_output = sys.argv[2]
        sw_address = sys.argv[3]
        sw_port = sys.argv[4]
        dev_id = sys.argv[5]

        control_plane = control_plane()
        try:
            control_plane.connect(sw_address, sw_port, dev_id, p4info_output, p4_output)
            print("Connected to switch")
        except Exception as e:
            print("Connection to switch failed")
            printGrpcError(e)
            sys.exit(2)

        try:
            control_plane.setup()
            print("Switch setup completed")
        except Exception as e:
            print("Switch setup failed")
            printGrpcError(e)
            sys.exit(2)

        try:
            control_plane.waitLoop()
        except:
            sys.exit(2)

