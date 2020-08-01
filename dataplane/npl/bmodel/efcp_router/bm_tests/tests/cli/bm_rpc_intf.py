"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

import socket
import struct

class BMRpcIntf(object):
    connected = False
    def __init__(self, server="localhost", port=9090, regsfile=""):
        self.client_socket = socket.socket();
        try:
            self.client_socket.connect((server,port))
            self.connected = True
        except socket.error, exc:
            print("Socket (%s:%s) Error : %s" % (server, port, exc))

    def is_connected(self):
        if self.connected:
            return True
        return False

    def __del__(self):
        self.client_socket.close()

    tx_hdr_format = "!iii"
    tx_hdr_size = struct.calcsize(tx_hdr_format)

    rx_hdr_format = "!iiiQQ"
    rx_hdr_size = struct.calcsize(rx_hdr_format)

    def issue_cmd(self, pkt):
        """
        Send packet with command and return the response.
        """
        try:
            self.client_socket.send(str(pkt))
            recv_pkt = self.client_socket.recv(4096)
            (port, pkt_len, pkt_dropped, itime, etime) \
                = struct.unpack(BMRpcIntf.rx_hdr_format,
                                recv_pkt[:BMRpcIntf.rx_hdr_size])
            return {"PACKET": recv_pkt[BMRpcIntf.rx_hdr_size:],
                    "PORT": port, "STATUS": pkt_dropped,
                    "ING_TIME": itime, "EGR_TIME": etime}
        except socket.error, exc:
            print("Socket Error : %s" % exc)
            return {"STATUS": 1}

    def tx_pkt(self, port, pkt, pkt_len):
        opcode = 11
        hdr = (struct.pack(BMRpcIntf.tx_hdr_format, opcode, port, pkt_len))
        pkt_buff = hdr + str(pkt)
        return self.issue_cmd(pkt_buff)

    def rx_pkt(self, port=0, pkt=0, pkt_len=0, ing_time=0, egr_time=0):
        opcode = 12
        hdr = (struct.pack(BMRpcIntf.tx_hdr_format, opcode, port, pkt_len))
        pkt_buff = hdr + str(pkt)
        return self.issue_cmd(pkt_buff)
