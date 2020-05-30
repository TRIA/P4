"""
nplsim test script
"""

# Packet configuration for tests
from nplconfig_base import *
from npltest_base import NPLBaseTest
from npltest_base import npl_test_run
from scapy.all import *


class Test(NPLBaseTest):
    """
    Test class
        connect to nplsim, run test, disconnect from nplsim
    """

    def __get_tx_packet(self, test_num, mac_src_addr, mac_dst_addr, ip_src_addr, ip_dst_addr):
        pkt = Ether()/IP()/TCP()/Raw()
	pkt[Ether].dst = mac_dst_addr
	pkt[Ether].src = mac_src_addr
	pkt[IP].dst = ip_dst_addr
	pkt[IP].src = ip_src_addr
	pkt[IP].ihl = ihl
	pkt[TCP].dataofs = dataofs
	pkt[Raw].load = "This packet is being sent by test {} from CLI to BM".format(test_num)
	return pkt

    # IMPORTANT: all tests must follow the pattern "test_*" in their names
    def test_pkt_transmitted_is_received(self):
        """
        Verify that an EFCP packet tagged with a VLAN and being transmitted to port X is received in this same port
        """
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
    npl_test_run(__name__, Test)
