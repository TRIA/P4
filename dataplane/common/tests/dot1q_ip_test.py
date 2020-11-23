"""
sim test script
"""

# Packet configuration for tests
from config_base import *
from test_base import BaseTest
from test_base import _test_run
from scapy.all import *
import pdu_types

# ScaPy initialisation
bind_layers(Ether, IP, type=0x0800)
bind_layers(Ether, Dot1Q, type=0x8100)
bind_layers(Dot1Q, IP, type=0x0800)


class Test(BaseTest):
    """
    Test class
        connect to sim, run test, disconnect from sim
    """

    def __get_tx_packet(self, test_num, mac_src_addr, mac_dst_addr, ip_src_addr, ip_dst_addr):
        eth_pkt = Ether(dst=mac_dst_addr, src=mac_src_addr, type=0x8100)
        pkt = eth_pkt

        # Note Dot1Q.id is really DEI (aka CFI)
        dot1q_pkt = Dot1Q(prio=0, id=0, vlan=random(0, 4095))
        pkt = pkt / dot1q_pkt
        pkt[Dot1Q:i].type = 0x8100
        pkt.type = 0x8100
       
        ip_pkt = IP(dst=mac_dst_addr, src=mac_src_addr, chksum=0x0000)
        ip_payload = "IP packet #{} sent from CLI to BM :)".format(test_num)
        pkt = pkt / ip_pkt / ip_payload

        codecs_decode_range = [ x for x in range(pktlen - len(pkt)) ]
        codecs_decode_str = "".join(["%02x"%(x%256) for x in range(pktlen - len(pkt)) ])
        codecs_decode = codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
        pkt = pkt / codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
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
