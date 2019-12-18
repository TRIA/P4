"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

import socket
import time

class bm_mgmt_intf(object):
    def __init__(self, server="localhost", port=9090, num_tries=10):
        self.client_socket = socket.socket();
        for attempt in range(num_tries):
            try:
                self.client_socket.connect((server,port))
                return
            except socket.error, exc:
                if attempt == num_tries-1:
                    print("Socket Error : %s" % exc)
                    self.client_socket = None
                    return 
            time.sleep(1)
        self.client_socket = None

    def __del__(self):
        if self.client_socket != None:
            self.client_socket.close()

    def tx_pkt(self, buff, buff_len):
        pkt_buff = str(buff)
        try:
            self.client_socket.send(str(pkt_buff))
            return {"STATUS": 0}
        except socket.error, exc:
            print("Socket Error : %s" % exc)
            return {"STATUS": 1}

    def rx_pkt(self):
        try:
            recv_pkt = self.client_socket.recv(2048);
            return {"PACKET": recv_pkt,"STATUS": 0}
        except socket.error, exc:
            print("Socket Error : %s" % exc)
            return {"STATUS": 1}
