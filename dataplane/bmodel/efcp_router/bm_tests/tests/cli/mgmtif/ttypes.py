"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

class PKT_STATUS:
  SUCCESS = 0
  DROPPED = 1

  _VALUES_TO_NAMES = {
    0: "SUCCESS",
    1: "DROPPED",
  }

  _NAMES_TO_VALUES = {
    "SUCCESS": 0,
    "DROPPED": 1,
  }


class lt_pt_data:
  """
  Attributes:
   - unit
   - sid
   - typ
   - index
   - buf_len
   - buf
  """
  def __init__(self, unit=None, sid=None, typ=None, index=None, buf_len=None, buf=None,):
    self.unit = unit
    self.sid = sid
    self.typ = typ
    self.index = index
    self.buf_len = buf_len
    self.buf = buf

class pkt_info:
  """
  Attributes:
   - status
   - port
   - pkt_len
   - pkt_buf
  """

  def __init__(self, status=None, port=None, pkt_len=None, pkt_buf=None,):
    self.status = status
    self.port = port
    self.pkt_len = pkt_len
    self.pkt_buf = pkt_buf
    return

