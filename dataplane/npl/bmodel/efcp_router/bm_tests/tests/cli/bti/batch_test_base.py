"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
batch test infrasctructure
"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "../.."))

from cli.bm_sim_intf import BmSimIf

def socket_port_get(argv):
    """
    Socket port number
    """
    socket_port = 9090

    for idx in range(1, len(argv), 2):
        argstr = argv[idx]
        if argstr == "-port" and idx+1 < len(argv):
            port = argv[idx+1]
            try:
                socket_port = int(port)
            except ValueError:
                pass

    return socket_port


def reg_file_get(argv):
    """
    Register file
    """
    reg_file = None

    for idx in range(1, len(argv), 2):
        argstr = argv[idx]
        if argstr == "-fe" and idx+1 < len(argv):
            reg_file = argv[idx+1] + "/bmodel/dfiles/bm_all.yml"

    return reg_file


class BatchTestBase(object):
    """
    Batch mod test infrastructure
    """
    bm_sim_if = None


    def __init__(self):
        pass

    def connect(self, host, socket_port, regfile):
        """
        Connect to BM SIM
            return True: connected; False: not connected
        """
        self.bm_sim_if = BmSimIf(host, socket_port, regfile)
        if not self.bm_sim_if.is_connected():
            print("ERROR Connecting to device")
            return False
        return True

    def configure(self, cfg_file):
        """
        Configure file
        """
        if self.bm_sim_if == None:
            print("Not connected to device")
            return -1

        if not os.path.isfile(cfg_file):
            print("ERROR Invalid configuration file: {}".format(cfg_file))
            return -1

        return self.bm_sim_if.rcload(cfg_file)

    def connect_with_cfg(self, host, socket_port, regfile, cfgfile):
        """
        Connect to BM SIM and configure with table configuration file
            return True: connected; False: not connected
        """
        self.connect(host, socket_port, regfile)
        status = self.configure(cfgfile)
        return (True, status)

    def send_pkt(self, port, pkt, pkt_len):
        """
        Send packet to BM
            return array of received packets
        """
        rcv_pkts = []

        if self.bm_sim_if == None:
            print("Not connected to device")
            recv_pkt = {}
            recv_pkt["STATUS"] = -1
            rcv_pkts.append(recv_pkt)
            return rcv_pkts

        recv_pkt = self.bm_sim_if.tx_pkt_cmd(port, pkt, pkt_len)

        rcv_pkts.append(recv_pkt)
        # Check tx status
        if recv_pkt["STATUS"] < 0:
            # TX ERROR
            return rcv_pkts

        #
        # Check remaining packets
        #
        while 1:
            recv_pkt = self.bm_sim_if.rx_pkt_cmd()
            if recv_pkt["STATUS"] < 0:
                # RX ERROR
                rcv_pkts.append(recv_pkt)
                continue

            if recv_pkt["PORT"] == -1:
                # No more packets
                break

            # RX packet
            rcv_pkts.append(recv_pkt)

        # Return packet array
        return rcv_pkts

    def process_txt_cmd(self, cmd):
        """
        Access to process text command
        """
        if self.bm_sim_if == None:
            print("Not connected to device")
            return (-1, None)

        return self.bm_sim_if.process_txt_cmd(cmd)

    def tx_pkt_cmd(self, port, pkt, pkt_len):
        """
        TX (transmitted) packet
        """
        if self.bm_sim_if == None:
            print("Not connected to device")
            return

        self.bm_sim_if.tx_pkt_cmd(port, pkt, pkt_len)

    def rx_pkt_cmd(self, port=0, pkt=0, pkt_len=0):
        """
        RX (received) packet
        """
        if self.bm_sim_if == None:
            print("Not connected to device")
            return

        return self.bm_sim_if.rx_pkt_cmd(port, pkt, pkt_len)

    def tx_pkt_hdr_info_get(self):
        """
        Get TX (transmitted) header info
        """
        return self.bm_sim_if.tx_pkt_hdr_info_get()

    def rx_pkt_hdr_info_get(self):
        """
        Get RX (received) header info
        """
        return self.bm_sim_if.rx_pkt_hdr_info_get()

    def issue_pkt_cmd(self, pkt):
        """
        Send packet with command and return the response.
        """
        return self.bm_sim_if.issue_pkt_cmd(pkt)

    def exit(self):
        """
        Disconnect from BM
        """
        if self.bm_sim_if == None:
            print("Not connected to device")
            return

        # Shutdown bm
        self.bm_sim_if.process_txt_cmd("exit -s")

        # Disconnect from NPLSIM
        self.bm_sim_if.disconnect()
