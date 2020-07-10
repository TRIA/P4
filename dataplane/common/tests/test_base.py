from scapy.all import *
import importlib
import os
import unittest
import sys

sys.path.append(os.path.dirname(__file__))

# Table configuration file located at the root
CFG_FILE = os.path.join(os.path.dirname(__file__), "tbl_cfg.txt")
REG_FILE = os.path.realpath(os.path.join(os.path.dirname(__file__), "../../fe_output/bmodel/dfiles/", "bm_all.yml"))


class BaseTest(unittest.TestCase):
    """
    Base test class (any test should inherit this)
        connect to model, run test, disconnect from model
    """
    bt_if = None
    connected = False
    batch_mode = True

    def __init__(self, batch_mode, host, sport, reg_file, cfg_file):
        self.batch_mode = batch_mode
        self.bt_if = BatchTestBase()

        # TODO: connect here
        #self.connected = self.bt_if.connect(host, sport, reg_file)
        #self.connected, self.cli_status = self.bt_if.connect_with_cfg(host, sport, reg_file, cfg_file)

        self. __print_header("Loading table configuration")
        if self.batch_mode:
            # If batch_mode, then load config file
            #self.cli_status = self.bt_if.configure(cfg_file)
            (stat, data) = self.bt_if.process_txt_cmd("debug_level 4")

    def __print_colour(self, text, colour):
        print("{}{}\033[0m".format(colour, text))

    def __print_header(self, text, colour="\033[94m"):
        print("\n\n")
        self.__print_colour("{}...".format(text), colour)
        self.__print_colour("-" * len(text + "..."), colour)

    def assert_equal(self, expected, provided):
        try:
            self.assertTrue(expected == provided)
        except Exception as e:
            self.__print_colour("Error: data should be {}, instead found {}".format(expected, provided), "\033[91m")
            raise e

    def assert_contained_in(self, whole, part):
        try:
            self.assertTrue(part in whole)
        except Exception as e:
            self.__print_colour("Error: data {} should be contained in {}, but it is not".format(part, whole), "\033[91m")
            raise e

    def __get_tx_packet(self, test_num):
        raise NotImplementedError("Must be implemented in test class")

    def test_pkt_transmitted_is_received(self, fun_get_tx_packet, opts, layer_name, layer_dst_field_name, src_addr, dst_addr, dst_port):
        # List of ports to send packets to
        ports = [1]

        # Number of packets to be sent
        numpkts = 1

        # Fill optional variab;es
        mac_src_addr = opts.get("mac_src_addr", "")
        mac_dst_addr = opts.get("mac_dst_addr", "")

        for count in range(numpkts):
            for port in ports:
                # Get packet
                ##tx_pkt = self.__get_tx_packet(port+1, dst_addr, src_addr)
                #tx_pkt = fun_get_tx_packet(port+1, dst_addr, src_addr)
                tx_pkt = fun_get_tx_packet(port+1, mac_src_addr, mac_dst_addr, src_addr, dst_addr)

                print("--- Submitting packet(s)")
                print("TX PKT num #{} to port {}:".format(count, port))
                hexdump(tx_pkt)

                # Transmit packet
                recv_pkts = self.bt_if.send_pkt(port, tx_pkt, len(str(tx_pkt)))

                print("--- Receiving packet(s)")
                for rx_pkt in recv_pkts:
                    # Check tx status
                    if rx_pkt["STATUS"] < 0:
                        print("TX ERROR")
                        continue

                    # Check receive packet status
                    if rx_pkt["STATUS"]:
                        print("Packet is dropped!")
                        continue

                    print("RX PKT in port {}:".format(rx_pkt["PORT"]))
                    hexdump(rx_pkt["PACKET"])

                    # Verify that the payload of each sent packet is present at each received packet
                    # - Note: not use of equals but a substring in the emitted packet due to the presence of multiple
                    # - non-ASCII symbols in the received payload and because this may be incomplete as well
                    payload_excerpt = str(tx_pkt.getlayer(layer_name).payload)[:20]
                    self.assert_contained_in(str(rx_pkt["PACKET"]), payload_excerpt)
                    # Verify that the port where the packet was submitted to is correct
                    self.assert_equal(dst_addr, tx_pkt.getlayer(layer_name).fields[layer_dst_field_name])
                    # Verify that the port where the packet was received from is correct
                    self.assert_equal(dst_port, rx_pkt["PORT"])

    def run(self):
        """
        Main body of the test
        """
        self. __print_header("Initialising tests")

        # Obtain list of available tests (their name must start with "test_")
        fields_list = [x for x in dir(self)]
        test_methods_list = list(filter(lambda x: x.startswith("test_"), fields_list))
        for test_method_name in test_methods_list:
            test_method = getattr(self, test_method_name)
            print("###### Test: {}".format(test_method_name))
            test_method()

        self. __print_header("Ending tests with success", "\033[92m")

    def exit(self):
        """
        Disconnect from sim
        """
        if self.batch_mode:
            # if batch_mode then close sim,
            self.bt_if.exit()


def _test_init():
    # Get socket port
    socket_port = socket_port_get(sys.argv)

    # Get register file
    #reg_file = reg_file_get(sys.argv)
    reg_file = reg_file_get(REG_FILE)
    batch_mode = True

    if reg_file == None:
        batch_mode = False
    return socket_port, reg_file, batch_mode, CFG_FILE


def _test_run(specific_module, specific_class):
    socket_port, reg_file, batch_mode, _ = _test_init()

    # Make instance of the "specific_module" and the "specific_class" class object passed by parameter
    module = __import__(specific_module, fromlist=[str(specific_class)])
    # Connect to sim
    #test_if = specific_class(batch_mode, "localhost", socket_port, reg_file, CFG_FILE)
    test_if = specific_class(batch_mode, "localhost", socket_port, REG_FILE, CFG_FILE)

    # run tests
    test_if.run()

    # close sim
    test_if.exit()

if __name__ == "__main__":
    _test_run(__name__, Test)
