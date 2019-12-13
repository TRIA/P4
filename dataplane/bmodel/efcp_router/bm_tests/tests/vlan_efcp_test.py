"""
nplsim test script
"""

from scapy.all import *
import os
import sys

BATCH_TEST_DIR = os.path.join(os.path.dirname(__file__), os.environ['NCS_ROOT'] + '/bmi/cli/bti')
sys.path.append(BATCH_TEST_DIR)
from batch_test_base import BatchTestBase, socket_port_get, reg_file_get

CFG_FILE = 'bm_tests/l2_test/tbl_cfg.txt'

#Pkt config
dst_mac = "00:00:05:00:00:02"
src_mac = "00:00:05:00:00:01"

class EFCP(Packet):
    name = "EFCP"

    # XByteField --> 1 Byte-integer (X bc the representation of the fields
    #                               value is in hexadecimal)
    #ShortField --> 2 Byte-integer
    #IntEnumField --> 4 Byte-integer

    fields_desc = [ XByteField("version", 0x01),
                    ShortField("dst_addr", 0x00),
                    ShortField("scr_addr", 1),
                    XByteField("qos_id", 0x00),
                    ShortField("dst_cepid", 0),
                    ShortField("src_cepid", 0),
                    XByteField("pdutype", 0x00),
                    XByteField("flags", 0x00),
                    ShortField("length", 1),
                    IntField("seq_num", 0),
                    XShortField("chksum", 0x00)]

class VLAN(Packet):
    name = "VLAN"
    fields_desc = [ShortField("pcp_dei_id", 0x00), ShortField("etherType", 0xD1F)]

bind_layers(Ether, VLAN)
bind_layers(VLAN, EFCP, type=0x8100)

def get_tx_packet(test_num):
    pkt = Ether() / EFCP(dst_addr=2, pdutype=0x80)
    pkt = Ether(type=0x8100) / VLAN(etherType=0xD1F) / EFCP(dst_addr=2, pdutype=0x80)
    pkt[Ether].dst = dst_mac
    pkt[Ether].src = src_mac
    pkt = pkt/"This EFCP packet #{} from CLI to BM :)".format(test_num)
    return pkt

class Test(object):
    """ test class
        connect to nplsim, run test, disconnect from nplsim
    """
    bt_if = None
    connected = False
    batch_mode = True

    def __init__(self, batch_mode, host, sport, reg_file, cfg_file):
        self.batch_mode = batch_mode
        self.bt_if = BatchTestBase()

        self.connected = self.bt_if.connect(host, sport, reg_file)

        if self.batch_mode:
            # if batch_mode then load config file
            stat = self.bt_if.configure(cfg_file)

            (stat, data) = self.bt_if.process_txt_cmd('debug_level 4')
    

    def run(self):
        """ run test
        """
        # port list to send packets
        port = [1]

        # number of packets to send
        numpkts = 1

        for count in range(numpkts):

            for i in port:

                # get packet
                
                pkt = get_tx_packet(i+1)


                print "##################################"
                print "TX PKT num {} on port {}:".format(count, i)
                hexdump(pkt)

                # transmit packet
                recv_pkts = self.bt_if.send_pkt(i, pkt, len(str(pkt)))

                for rx_pkt in recv_pkts:
                    # check tx status
                    if rx_pkt['STATUS'] < 0:
                        print "TX ERROR"
                        continue

                    # check receive packet status
                    if rx_pkt['STATUS']:
                        print "Packet is Dropped!"
                        continue

                    print "RX PKT on port {}:".format(rx_pkt['PORT'])
                    hexdump(rx_pkt['PACKET'])


    def exit(self):
        """ disconnect from nplsim
        """
        if self.batch_mode:
            # if batch_mode then close nplsim,
            self.bt_if.exit()

if __name__ == '__main__':
    """ test.py -port <socket port num> -fe <output dir>
        if -fe is specified then running in batch_mode:
            configure nplsim tables
            terminate nplsim process
    """
    # get socket port
    socket_port = socket_port_get(sys.argv)

    # get register file
    reg_file = reg_file_get(sys.argv)
    batch_mode = True
    if reg_file == None:
        batch_mode = False

    # connect to nplsim
    test_if = Test(batch_mode, 'localhost', socket_port, reg_file, CFG_FILE)

    # run tests
    test_if.run()

    # close nplsim
    test_if.exit()            