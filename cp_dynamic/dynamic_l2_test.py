import grpc
import time
import os
import sys

this_file = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(this_file, '../config'))
sys.path.append(os.path.join(this_file, '../common/python/utils'))

from routerctl_pb2 import *
from routerctl_pb2_grpc import *
from cfg import *

with grpc.insecure_channel("localhost:50051") as channel:
    # MAC  = "00:00:00:00:00:01"
    # SMAC = "01:00:00:00:00:00"
    # m = routerctl_pb2.ReqAddMACToPort(mac=MAC, port=10)
    # stub = routerctl_pb2_grpc.L2RouterControlStub(channel)
    # stub.AddMACToPort(m)
    # m = routerctl_pb2.ReqAddSpoofedMAC(mac=MAC, spoofed_mac=SMAC)
    # stub.AddSpoofedMAC(m)
    # time.sleep(10)
    # m = routerctl_pb2.ReqDelMACToPort(mac=MAC)
    # stub.DelMACToPort(m)
    # m = routerctl_pb2.ReqDelSpoofedMAC(mac=MAC)
    # stub.DelSpoofedMAC(m)

    s = L2RouterControlStub(channel)

    s.AddMACToPort(ReqAddMACToPort(mac=EDF9_FAKE_MAC, port=PORT_A_NO))
    s.AddMACToPort(ReqAddMACToPort(mac=EDF10_FAKE_MAC, port=PORT_B_NO))
    s.AddSpoofedMAC(ReqAddSpoofedMAC(mac=EDF9_REAL_MAC, spoofed_mac=EDF9_FAKE_MAC))
    s.AddSpoofedMAC(ReqAddSpoofedMAC(mac=EDF10_REAL_MAC, spoofed_mac=EDF10_FAKE_MAC))

