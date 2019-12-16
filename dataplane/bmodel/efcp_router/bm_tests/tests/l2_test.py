"""
nplsim test script
"""

from npltest_base import NPLBaseTest
from npltest_base import npl_test_run
from scapy.all import *


# Packet configuration
mac_dst_addr = "00:00:05:00:00:01"
mac_src_addr = "00:00:05:00:00:00"
ip_dst_addr = "10.0.1.10"
ip_src_addr = "10.0.0.10"
ihl = 5
ip_len = 108
dataofs = 5


class Test(NPLBaseTest):
    """
    Test class
        connect to nplsim, run test, disconnect from nplsim
    """

    def __get_tx_packet(self, test_num):
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
        # list of ports to send packets to
        ports = [1]

        # number of packets to be sent
        numpkts = 1

        for count in range(numpkts):
            for port in ports:
                # get packet
                tx_pkt = self.__get_tx_packet(port+1)

                print("### Submitting packet(s)")
                print("TX PKT num #{} to port {}:".format(count, port))
                hexdump(tx_pkt)

                # transmit packet
                recv_pkts = self.bt_if.send_pkt(port, tx_pkt, len(str(tx_pkt)))

                print("### Receiving packet(s)")
                for rx_pkt in recv_pkts:
                    # check tx status
                    if rx_pkt["STATUS"] < 0:
                        print("TX ERROR")
                        continue

                    # check receive packet status
                    if rx_pkt["STATUS"]:
                        print("Packet is dropped!")
                        continue

                    print("RX PKT on port {}:".format(rx_pkt["PORT"]))
                    hexdump(rx_pkt["PACKET"])


if __name__ == "__main__":
    # IMPORTANT: do not change this line
    npl_test_run(__name__, Test)
