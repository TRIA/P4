"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""The client side CLI to communicate with Behavior Model (BM) server.

The client side Command Line Interface (CLI) communicate to the BM server by
TCP sockets. The CLI framework inherits the ClI class that supports
environment varaibles and various script commands.

The client side CLI stores the tables information to communicate to server
by loading a register file in YAML format. An "lt" CLI command is added
in this module to perform table operation.
"""

import os

from bmif_cli import BmifCli


def get_file_path(file_name):
    """
    Get full path filename
    """
    filename = os.path.abspath(file_name)
    return filename


def get_file_data(file_name):
    """
    Return file data
    """
    flptr = open(file_name, "r")
    data = flptr.read()
    return data


class TxtCmds(object):
    """
    Class to process cli commands
    """
    def __init__(self):
        pass

    def get_data(self, file_name):
        """
        Return data from specified file
            only return command strings
            pull in commands from any "rcload"
        """
        data = []

        # Read in file
        filename = get_file_path(file_name)
        filedata = get_file_data(filename)

        lines = filedata.split("\n")
        for line in lines:
            # Print line
            # Remove extra spaces
            nline = " ".join(line.split())
            nline = nline.split("#")[0].strip()
            if len(nline) > 0:
                if nline.startswith("rcload"):
                    fname = nline.split("rcload")[1].strip()
                    nline = self.get_data(fname)
                    for cmd in nline:
                        data.append(cmd)
                else:
                    data.append(nline)
        return data


class BmSimIf(object):
    """
    BM Simulation interface
    """
    bm_cli_if = None

    def __init__(self, server="localhost", port=9090, regsfile=""):
        if regsfile == "":
            print("ERROR: Missing regsfile")
            return

        self.bm_cli_if = BmifCli(noncliif=True)

        self.bm_cli_if.switch_connect(server, port)
        if self.bm_cli_if.client == None:
            return

        if regsfile != None:
            print("Reading {} ...".format(regsfile))
            self.bm_cli_if.tables_get_from_yml(regsfile)

    def is_connected(self):
        """
        Check if interface is connected
        """
        if self.bm_cli_if == None:
            return False

        return self.bm_cli_if.switch_is_connected()

    def disconnect(self):
        """
        Disconnect interface
        """
        if self.is_connected():
            self.bm_cli_if.switch_disconnect()

    def process_txt_cmd(self, cmd):
        """
        Process command
            return (status, data)
        """
        cmdops = {"exit": self.bm_cli_if.do_exit,
                  "quit": self.bm_cli_if.do_quit,
                  "debug_level": self.bm_cli_if.do_debug_level,
                  "disp_pkt": self.bm_cli_if.do_disp_pkt,
                  "print_stats": self.bm_cli_if.do_print_stats,
                  "pkt_has_crc": self.bm_cli_if.do_pkt_has_crc,
                  "crc_gen_en": self.bm_cli_if.do_crc_gen_en,
                  "ctr": self.bm_cli_if.do_ctr,
                  "lt": self.bm_cli_if.do_lt,
                  "pt": self.bm_cli_if.do_pt}

        self.bm_cli_if.clr_err()
        #print("process_txt_cmd: {}".format(cmd))

        if not self.is_connected():
            print("ERROR: No connection to BM SIM")
            return (-1, None)

        args = cmd.split(" ")
        if args[0] not in cmdops:
            print("ERROR: unknown command: {}".format(args[0]))
            return (-1, None)

        cmdop_fun = cmdops[args[0]]
        rv_data = cmdop_fun(args[1:])

        if self.bm_cli_if.has_err():
            return (-1, None)

        return (0, rv_data)

    def tx_pkt_cmd(self, port, pkt, pkt_len):
        """
        Transmit packet
        """
        if not self.is_connected():
            print("ERROR: No connection to BM SIM")
            return {"STATUS": -1}

        return self.bm_cli_if.tx_pkt(port, pkt, pkt_len)

    def rx_pkt_cmd(self, port=0, pkt=0, pkt_len=0):
        """
        Receive packet
        """
        if not self.is_connected():
            return {"STATUS": -1}

        return self.bm_cli_if.rx_pkt(port, pkt, pkt_len)

    def rcload(self, cfg_file):
        """
        Load config file
        """
        err = 0
        print("loading {} ...".format(cfg_file))
        data_if = TxtCmds()
        cfg_data = data_if.get_data(cfg_file)
        for cmd in cfg_data:
            # Process each text command
            (stat, data) = self.process_txt_cmd(cmd)
            if stat != 0:
                print("ERROR processing: {}".format(cmd))
                err = -1
        return err

    def tx_pkt_hdr_info_get(self):
        """
        Get transmitted (TX) header info
        """
        return self.bm_cli_if.tx_pkt_hdr_info_get()

    def rx_pkt_hdr_info_get(self):
        """
        Get received (RX) header info
        """
        return self.bm_cli_if.rx_pkt_hdr_info_get()

    def issue_pkt_cmd(self, pkt):
        """
        Send packet with command and return the response.
        """
        return self.bm_cli_if.issue_pkt_cmd(pkt)

