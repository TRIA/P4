"""
nplsim test script
"""

from nplclass_base import EFCP
# Packet configuration for tests
from nplconfig_base import *
from npltest_base import NPLBaseTest
from npltest_base import npl_test_run
from scapy.all import *


# ScaPy initialisation
bind_layers(Ether, EFCP, type=0xD1F)


class Test(NPLBaseTest):
    """
    Test class
        connect to nplsim, run test, disconnect from nplsim
    """

    def __get_tx_packet(self, test_num, ipc_dst_addr, ipc_src_addr):
        pkt = Ether() / EFCP(ipc_dst_addr=ipc_dst_addr, ipc_src_addr=ipc_src_addr, pdutype=0x8001)
        pkt[Ether].dst = mac_dst_addr
        pkt[Ether].src = mac_src_addr        
        pkt = pkt/"EFCP packet #{} sent from CLI to BM :)".format(test_num)
        return pkt

    # IMPORTANT: all tests must follow the pattern "test_*" in their names
    def test_pkt_transmitted_is_received(self):
        """
        Verify that an EFCP packet being transmitted to port X is received in this same port
        """
        # Arguments to parameterise test
        test_args = {
            "layer_name": "EFCP",
            "layer_dst_field_name": "ipc_dst_addr",
            "src_addr": ipc_src_addr,
            "dst_addr": ipc_dst_addr,
            "dst_port": ipc_dst_addr,
        }
        # Optional arguments (e.g., used to populate the "__get_tx_packet" method passed by parameter)
        test_opts = {
            "mac_src_addr": mac_src_addr,
            "mac_dst_addr": mac_dst_addr,
        }
        # Execute test (logic in base class)
        super(Test, self).test_pkt_transmitted_is_received(self.__get_tx_packet, test_opts, **test_args)


if __name__ == "__main__":
    # IMPORTANT: do not change this line
    npl_test_run(__name__, Test)
