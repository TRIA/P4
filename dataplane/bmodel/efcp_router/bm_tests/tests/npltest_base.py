from scapy.all import *
import importlib
import os
import unittest
import sys

BATCH_TEST_DIR = os.path.join(os.path.dirname(__file__), os.environ["NCS_ROOT"] + "/bmi/cli/bti")
sys.path.append(BATCH_TEST_DIR)
from batch_test_base import BatchTestBase, socket_port_get, reg_file_get

CFG_FILE = "bm_tests/l2_test/tbl_cfg.txt"


class NPLBaseTest(unittest.TestCase):
    """
    Base test class (any test should inherit this)
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
            (stat, data) = self.bt_if.process_txt_cmd("debug_level 4")

    def __get_tx_packet(self, test_num):
        raise NotImplementedError("Must be implemented in test class")

    def run(self):
        """
        Main body of the test
        """
        print("############ STARTING TESTS\n")

        # Obtain list of available tests (their name must start with "test_")
        fields_list = [x for x in dir(self)]
        test_methods_list = list(filter(lambda x: x.startswith("test_"), fields_list))
        for test_method_name in test_methods_list:
            test_method = getattr(self, test_method_name)
            print("###### Test: {}".format(test_method_name))
            test_method()

        print("\n############ ENDING TESTS (SUCCESSFULLY)")

    def exit(self):
        """
        Disconnect from nplsim
        """
        if self.batch_mode:
            # if batch_mode then close nplsim,
            self.bt_if.exit()


def npl_test_init():
    # get socket port
    socket_port = socket_port_get(sys.argv)

    # get register file
    reg_file = reg_file_get(sys.argv)
    batch_mode = True
    if reg_file == None:
        batch_mode = False

    return socket_port, reg_file, batch_mode, CFG_FILE


def npl_test_run(specific_module, specific_class):
    socket_port, reg_file, batch_mode, _ = npl_test_init()

    # Make instance of the "specific_module" and the "specific_class" class object passed by parameter
    module = __import__(specific_module, fromlist=[str(specific_class)])
    # connect to nplsim
    test_if = specific_class(batch_mode, "localhost", socket_port, reg_file, CFG_FILE)

    # run tests
    test_if.run()

    # close nplsim
    test_if.exit()

if __name__ == "__main__":
    npl_test_run(__name__, Test)
