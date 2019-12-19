"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

import struct
from ttypes import *

class Client:
  def __init__(self, mgmt_intf, oprot=None):
    self._seqid = 0
    self._mgmt_intf = mgmt_intf

  def check_status_vector(self, expected_status):
    """
    Parameters:
     - expected_status
    """
    return self.send_check_status_vector(expected_status);

  def send_check_status_vector(self, expected_status):
    # Exit
    opcode = 1
    buf    = str(struct.pack("!ii", opcode, expected_status))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def bm_exit(self, cmd):
    """
    Parameters:
     - cmd
    """
    return self.send_bm_exit(cmd)

  def send_bm_exit(self, cmd):
    # Exit
    opcode = 2
    # 1 - Exit
    exit   = cmd
    buf    = str(struct.pack("!ii", opcode, exit))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def ngsdk_tbl_debug_set(self, debug_en):
    """
    Parameters:
     - debug_en
    """
    return self.send_ngsdk_tbl_debug_set(debug_en)

  def send_ngsdk_tbl_debug_set(self, debug_en):
    # Exit
    opcode = 3
    buf    = str(struct.pack("!ii", opcode, debug_en))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def ngsdk_p2l_set(self, data):
    """
    Parameters:
     - data
    """
    self.send_ngsdk_p2l_set(data)
    return self.recv_ngsdk_p2l_set()

  def send_ngsdk_p2l_set(self, data):
    # p2l_set
    opcode = 4
    hdr    = str(struct.pack("!iiiiii", opcode, data.unit, data.sid, data.typ, data.index, data.buf_len))
    buf    = hdr + data.buf
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_ngsdk_p2l_set(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    hdr_format = "!ii"
    (success, status) = struct.unpack(hdr_format, recv_pkt["PACKET"])
    return status

  def ngsdk_l2p_get(self, data):
    """
    Parameters:
     - data
    """
    self.send_ngsdk_l2p_get(data)
    return self.recv_ngsdk_l2p_get()

  def send_ngsdk_l2p_get(self, data):
    # p2l_set
    opcode = 5
    hdr    = str(struct.pack("!iiiiii", opcode, data.unit, data.sid, data.typ, data.index, data.buf_len))
    buf    = hdr + data.buf
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_ngsdk_l2p_get(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    hdr_format = "!iiiii"
    hdr_size = struct.calcsize(hdr_format)
    rd = lt_pt_data()
    (rd.unit,rd.sid,rd.typ,rd.index,rd.buf_len) = struct.unpack(hdr_format, recv_pkt["PACKET"][:hdr_size])
    rd.buf = recv_pkt["PACKET"][hdr_size:]
    status = rd.typ
    return (rd, status)

  def ngsdk_tbl_dump(self, data):
    """
    Parameters:
     - data
    """
    self.send_ngsdk_tbl_dump(data)
    return self.recv_ngsdk_tbl_dump()

  def send_ngsdk_tbl_dump(self, data):
    # p2l_set
    opcode = 6
    hdr    = str(struct.pack("!iiiii", opcode, data.unit, data.sid, data.index, data.buf_len))
    buf    = hdr + data.buf
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_ngsdk_tbl_dump(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    return

  def add_port(self, if_name):
    """
    Parameters:
     - if_name
    """
    return self.send_add_port(if_name)

  def send_add_port(self, if_name):
    return

  def crc_gen_en(self, crc_en):
    """
    Parameters:
     - crc_en
    """
    self.send_crc_gen_en(crc_en)
    return self.recv_crc_gen_en()

  def send_crc_gen_en(self, crc_en):
    # Exit
    opcode = 8
    buf    = str(struct.pack("!ii", opcode, crc_en))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_crc_gen_en(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    return

  def pkt_has_crc(self, crc):
    """
    Parameters:
     - crc
    """
    self.send_pkt_has_crc(crc)
    return self.recv_pkt_has_crc()

  def send_pkt_has_crc(self, crc):
    # Exit
    opcode = 9
    buf    = str(struct.pack("!ii", opcode, crc))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_pkt_has_crc(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    return

  def port_status_get(self, port, port_stats):
    """
    Parameters:
     - port
     - port_stats
    """
    self.send_port_status_get(port, port_stats)
    return self.recv_port_status_get()

  def send_port_status_get(self, port, port_stats):
    return

  def recv_port_status_get(self):
    return

  def tx_pkt_to_port(self, port, pkt, pkt_len):
    """
    Parameters:
     - pkt
    """
    self.send_tx_pkt_to_port(port, pkt, pkt_len)
    return self.recv_tx_pkt_to_port()

  def send_tx_pkt_to_port(self, port, pkt, pkt_len):
    opcode = 11
    hdr_format = "!iii"
    hdr = struct.pack(hdr_format, opcode, port, pkt_len)
    pkt_buff = hdr + str(pkt)
    self._mgmt_intf.tx_pkt(pkt_buff, len(pkt_buff))
    return

  def recv_tx_pkt_to_port(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    if recv_pkt["STATUS"] == 1:
        # Error
        return {"STATUS": 1}

    recv_hdr_format = "!iiiQQ"
    recv_hdr_size = struct.calcsize(recv_hdr_format)

    # Return packet data
    (port, pkt_len, pkt_dropped, itime, etime) = struct.unpack(recv_hdr_format, recv_pkt["PACKET"][:recv_hdr_size])
    return {"PACKET": recv_pkt["PACKET"][recv_hdr_size:], "PORT": port, "STATUS": pkt_dropped, \
            "ING_TIME": itime, "EGR_TIME": etime}

  def rx_pkt_from_port(self, port=0, pkt=0, pkt_len=0):
    self.send_rx_pkt_from_port(port, pkt, pkt_len)
    return self.recv_rx_pkt_from_port()

  def send_rx_pkt_from_port(self, port, pkt, pkt_len):
    opcode = 12
    hdr_format = "!iii"
    hdr = struct.pack(hdr_format, opcode, port, pkt_len)
    pkt_buff = hdr + str(pkt)
    self._mgmt_intf.tx_pkt(pkt_buff, len(pkt_buff))
    return

  def recv_rx_pkt_from_port(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    if recv_pkt["STATUS"] == 1:
        # Error
        return {"STATUS": 1}

    recv_hdr_format = "!iiiQQ"
    recv_hdr_size = struct.calcsize(recv_hdr_format)

    # Return data
    (port, pkt_len, pkt_dropped, itime, etime) = struct.unpack(recv_hdr_format, recv_pkt["PACKET"][:recv_hdr_size])
    return {"PACKET": recv_pkt["PACKET"][recv_hdr_size:], "PORT": port, "STATUS": pkt_dropped, \
            "ING_TIME": itime, "EGR_TIME": etime}

  def tx_pkt_hdr_info_get(self):
    return ("!iii", struct.calcsize("!iii"))

  def rx_pkt_hdr_info_get(self):
    return ("!iiiQQ", struct.calcsize("!iiiQQ"))

  def issue_pkt_cmd(self, pkt):
    """
    Send packet with command and return the response.
    """
    try:
        self._mgmt_intf.tx_pkt(str(pkt), len(str(pkt)))
        recv_pkt = self._mgmt_intf.rx_pkt()

        # Return packet data
        (port, pkt_len, pkt_dropped, itime, etime) = struct.unpack("!iiiQQ", recv_pkt["PACKET"][:struct.calcsize("!iiiQQ")])
        return {"PACKET": recv_pkt["PACKET"][struct.calcsize("!iiiQQ"):], "PORT": port, "STATUS": pkt_dropped, \
            "ING_TIME": itime, "EGR_TIME": etime}

    except socket.error, exc:
        print("Socket Error : %s" % exc)
        return {"STATUS": 1}

  def print_stats(self, port):
    """
    Parameters:
     - port
    """
    return self.send_print_stats(port)

  def send_print_stats(self, port):
    opcode = 13 #exit
    buf    = str(struct.pack("!ii", opcode, port))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def disp_pkt(self, disp):
    """
    Parameters:
     - disp
    """
    self.send_disp_pkt(disp)
    return self.recv_disp_pkt()

  def send_disp_pkt(self, disp):
    # Exit
    opcode = 14
    buf    = str(struct.pack("!ii", opcode, disp))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_disp_pkt(self):
    return

  def debug_level_get(self):
    """
    Parameters:
     - debug_level_get
    """
    self.send_debug_level_get()
    return self.recv_debug_level_get()

  def send_debug_level_get(self):
    # Exit
    opcode = 15
    buf    = str(struct.pack("!i", opcode))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_debug_level_get(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    hdr_format = "!i"
    hdr_size = struct.calcsize(hdr_format)
    (level,) = struct.unpack(hdr_format, recv_pkt["PACKET"][:hdr_size])
    return level

  def debug_level_set(self, lvl):
    """
    Parameters:
     - debug_level_set
    """
    self.send_debug_level_set(lvl)
    return self.recv_debug_level_set()

  def send_debug_level_set(self, lvl):
    # Exit
    opcode = 16
    buf    = str(struct.pack("!ii", opcode, lvl))
    self._mgmt_intf.tx_pkt(buf, len(buf))
    return

  def recv_debug_level_set(self):
    recv_pkt = self._mgmt_intf.rx_pkt()
    return

