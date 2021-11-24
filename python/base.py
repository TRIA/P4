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

    def waitLoop(self):
        while True:
            time.sleep(10)

def ControlPlaneMain(control_plane):
    if len(sys.argv) < 3:
        print("Usage: ./p4runtime.py <address>:<port> <device id>")
        sys.exit(1)
    else:
        sw_address = sys.argv[1]
        sw_port = sys.argv[2]
        dev_id = sys.argv[3]

        control_plane = control_plane()
        try:
            control_plane.connect(sw_address, sw_port, dev_id, P4INFO_FILE_PATH, TOFINO_BIN_PATH)
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

