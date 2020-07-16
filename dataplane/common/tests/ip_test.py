"""
sim test script
"""

# Packet configuration for tests
from config_base import *
from test_base import BaseTest
from test_base import _test_run
from scapy.all import *


class Test(BaseTest):
    """
    Test class
        connect to sim, run test, disconnect from sim
    """

    def __get_tx_packet(self, test_num, mac_src_addr, mac_dst_addr, ip_src_addr, ip_dst_addr):
        pkt = Ether()/IP()/Raw()
        pkt[Ether].dst = mac_dst_addr
        pkt[Ether].src = mac_src_addr
        pkt[IP].dst = ip_dst_addr
        pkt[IP].src = ip_src_addr
        pkt[IP].chksum = 0x0000
        pkt[Raw].load = "IP packet #{} sent from CLI to BM :)".format(test_num)
        return pkt

    # IMPORTANT: all tests must follow the pattern "test_*" in their names
    def test_pkt_transmitted_is_received(self):
        """
        Verify that an IP packet being transmitted to port X is received in this same port
        """
        # Destination IPv4 address
        ip_dst_addr = "10.0.0.2"

        # Destination IPv4 address
        ip_src_addr = "10.0.0.1"

        # Destination port where the packet is received
        port_dst = 2

        # Arguments to parameterise test
        test_args = {
            "layer_name": "IP",
            "layer_dst_field_name": "dst",
            "src_addr": ip_src_addr,
            "dst_addr": ip_dst_addr,
            "dst_port": port_dst,
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
    _test_run(__name__, Test)
