"""
nplsim test script
"""

from nplclass_base import EFCP
from npltest_base import NPLBaseTest
from npltest_base import npl_test_run
from scapy.all import *


# Packet configuration
mac_dst_addr = "00:00:05:00:00:02"
mac_src_addr = "00:00:05:00:00:01"

# ScaPy initialisation
bind_layers(Ether, EFCP, type=0xD1F)


class Test(NPLBaseTest):
    """
    Test class
        connect to nplsim, run test, disconnect from nplsim
    """

    def __get_tx_packet(self, test_num, ipc_dst_addr):
        pkt = Ether() / EFCP(ipc_dst_addr=ipc_dst_addr, pdutype=0x80)
        pkt[Ether].dst = mac_dst_addr
        pkt[Ether].src = mac_src_addr
        pkt = pkt/"EFCP packet #{} sent from CLI to BM :)".format(test_num)
        return pkt

    # IMPORTANT: all tests must follow the pattern "test_*" in their names
    def test_pkt_transmitted_is_received(self):
        """
        Verify that an EFCP packet being transmitted to port X is received in this same port
        """
        # list of ports to send packets to
        ports = [1]

        # destination IPC address
        ipc_dst_addr = 2

        # number of packets to be sent
        numpkts = 1

        for count in range(numpkts):
            for port in ports:
                # get packet
                tx_pkt = self.__get_tx_packet(port+1, ipc_dst_addr)

                print("--- Submitting packet(s)")
                print("TX PKT num #{} to port {}:".format(count, port))
                hexdump(tx_pkt)

                # transmit packet
                recv_pkts = self.bt_if.send_pkt(port, tx_pkt, len(str(tx_pkt)))

                print("--- Receiving packet(s)")
                for rx_pkt in recv_pkts:
                    # check tx status
                    if rx_pkt["STATUS"] < 0:
                        print("TX ERROR")
                        continue

                    # check receive packet status
                    if rx_pkt["STATUS"]:
                        print("Packet is dropped!")
                        continue

                    print("RX PKT in port {}:".format(rx_pkt["PORT"]))
                    hexdump(rx_pkt["PACKET"])

                    # Verify that the payload of each sent packet is present at each received packet
                    # - Note: not use of equals but a substring in the emitted packet due to the presence of multiple
                    # - non-ASCII symbols in the received payload and because this may be incomplete as well
                    efcp_payload_excerpt = str(tx_pkt.getlayer("EFCP").payload)[:20]
                    self.assertTrue(efcp_payload_excerpt in str(rx_pkt["PACKET"]))
                    # Verify that the port where the packet was submitted to is correct
                    self.assertTrue(ipc_dst_addr == tx_pkt.getlayer("EFCP").fields["ipc_dst_addr"])


if __name__ == "__main__":
    # IMPORTANT: do not change this line
    npl_test_run(__name__, Test)
