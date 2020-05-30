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

import sys, getopt, os.path
import yaml
import time
import re
import binascii

from mgmtif import Switch
from mgmtif import bm_mgmt_intf
from mgmtif.ttypes import lt_pt_data

from ltm.tablesdb import TablesDB
from ltm.tablesdb import format_val
from ltm.table import Table

from cli.cli import Cli

try:
    from yaml import CLoader as Loader
    from yaml import CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper
    print("WARN: Using slow YAML Load/Dump (please install libyaml)")

class BmifCli(Cli):
    """
    CLI client for Behavior Model.
    """

    def __init__(self, completekey="tab", stdin=None, stdout=None, prompt=None, noncliif=False):
        Cli.__init__(self, completekey, stdin, stdout, prompt)
        self.tdb = TablesDB()
        self.client = None
        self.transport = None
        self.error = False
        self.noncliif = noncliif

    def has_err(self):
        """
        Check if error occured
        """
        return self.error

    def clr_err(self):
        """
        Clear errors
        """
        self.error = False

    def err_msg(self, msg):
        """
        Print error message
        """
        self.error = True
        self.out(msg)

    def out_msg(self, msg):
        """
        Print console message
        """
        self.out(msg)

    def __switch_exit(self):
        "Send RPC command to terminate the BM server."
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.bm_exit(1)

    def __switch_crc_gen_en(self, enable):
        """
        Send RPC command to enable CRC generation in BM server.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.crc_gen_en(enable)

    def __switch_pkt_has_crc(self, crc):
        """
        Send RPC command to BM server to indicate whether packet has CRC.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.pkt_has_crc(crc)

    def __switch_print_stats(self, port):
        """
        Send RPC command to BM server to print statistics.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.print_stats(port)

    def __switch_disp_pkt(self, disp):
        """
        Send RPC command to BM server to display packets.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.disp_pkt(disp)

    def __switch_p2l_set(self, sid, typ, index, val, entry_size):
        """
        Send RPC command to BM server to set value of a table entry.
        """
        if self.client is None:
            raise RuntimeError("ERROR: Switch is not connected")
        datalist = [0] * entry_size
        bufsize = 0
        while bufsize < entry_size:
            byte = val & 0xff
            datalist[bufsize] = byte
            val = val >> 8
            bufsize += 1
            if val == 0:
                break
        databuf = bytearray(datalist)
        ngsdk_data = lt_pt_data(0, sid, typ, index, entry_size, databuf)
        status = self.client.ngsdk_p2l_set(ngsdk_data)
        return status

    def __switch_l2p_get(self, sid, typ, index, val, entry_size):
        """
        Send RPC command to BM server to get value from a table entry.
        """
        if self.client is None:
            raise RuntimeError("ERROR: Switch is not connected")
        datalist = [0] * entry_size
        bufsize = 0
        while bufsize < entry_size:
            byte = val & 0xff
            datalist[bufsize] = byte
            val = val >> 8
            bufsize += 1
            if val == 0:
                break
        databuf = bytearray(datalist)
        ngsdk_data = lt_pt_data(0, sid, typ, index, entry_size, databuf)
        (l2p_get, status) = self.client.ngsdk_l2p_get(ngsdk_data)

        val = 0
        if not status:
            for idx in range(entry_size):
                byte = ord(l2p_get.buf[idx])
                val |= byte << (idx * 8)
        return (val, status)

    def __switch_dbg_lvl_get(self):
        """
        Send RPC command to BM server to get debug level.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return None
        return self.client.debug_level_get()

    def __switch_dbg_lvl_set(self, lvl):
        """
        Send RPC command to BM server to set debug level.
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected\n")
            return
        self.client.debug_level_set(lvl)

    def __switch_tx_pkt(self, port, pkt, pkt_len):
        """
        Send RPC command to BM server to TX packet
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected")
            return
        return self.client.tx_pkt_to_port(port, pkt, pkt_len)

    def __switch_rx_pkt(self, port=0, pkt=0, pkt_len=0):
        """
        Send RPC command to BM server to RX packet
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected")
            return
        return self.client.rx_pkt_from_port(port, pkt, pkt_len)

    def print_status(self, name, typ, status):
        """
        Print status
        """
        if typ == 0:
            op = "insert"
        elif typ == 1:
            op = "update"
        elif typ == 2:
            op = "delete"
        elif typ == 3:
            op = "lookup"
        else:
            op = "Unknown"
            self.err_msg("ERROR: Invalid typ used. %d\n" % (typ))

        # All good. Print fields
        if status == 0:
            pass
        # Entry not found
        elif status == -1:
            #ERROR: lt L2_HOST_TABLE lookup: Entry not found
            self.err_msg("ERROR: lt %s %s: Entry not found.\n" % (name, op))
        # Entry exists
        elif status == -2:
            #ERROR: lt MY_STATION_TABLE insert: Entry exists
            self.err_msg("ERROR: lt %s %s: Entry exists.\n" % (name, op))
        # Table full
        elif status == -3:
            #ERROR: lt IPINIP_TUNNEL_DEFAULT_TABLE insert: Table full
            self.err_msg("ERROR: lt %s %s: Table full.\n" % (name, op))
        # Unknown error
        else:
            self.err_msg("ERROR: lt %s %s: error occured.\n" % (name, op))

    def __lt_list(self, *args):
        """
        List specified table information
        """
        num_args = len(args)
        list_format = None
        idx = 0
        # Check for options
        options = {"-l":"long", "-b":"brief"}
        while idx < num_args and args[idx][0] == "-":
            if args[idx] in options:
                list_format = options[args[idx]]
            else:
                self.err_msg("ERROR: Option %s is not supported.\n" % (args[idx]))
                return
            idx += 1

        list_tables = []
        if idx == num_args:
            # Get all tables if nothing is specified.
            list_tables = self.tdb.tables_search("*", "npl")
        else:
            for arg in args[idx:]:
                list_tables += self.tdb.tables_search(arg, "npl")

        if len(list_tables) == 0:
            self.out_msg("No matching logical tables.\n")
            return
        for table in list_tables:
            self.out_msg("%s" % (self.tdb.table_info_get(table, list_format)))
        return

    def __lt_op_parse_field(self, table, *args):
        """
        Get the field set values from input syntax <field>=<val>

        Field _INDEX is a mandatory field in table operation.
        This method also checks whether _INDEX exists and verifies its value.
        """
        lt_key_fill = False
        if len(args) > 0 and args[0] == "-a":
            lt_key_fill = True
            args = args[1:]

        fsets = {}
        for arg in args:
            try:
                fname, val_str = arg.split("=", 1)
            except ValueError:
                self.err_msg("ERROR: Invalid field assignment: %s\n" % (arg))
                return None, None

            # Check for field name
            field = "_PRIORITY" if fname.lower() == "entry_priority" else table.find(fname)
            if field is None:
                self.err_msg("ERROR: Field %s is not valid for table/register %s\n" \
                         % (fname, table.name))
                return None, None

            # Check for field value
            try:
                val = int(val_str, 0)
            except ValueError:
                self.err_msg("ERROR: Invalid field assignment: %s\n" % (arg))
                return None, None
            fsets[field] = val

        # Check if index is valid
        idxfield = None
        for fset in fsets:
            if fset.upper() == "_INDEX":
                idxfield = fset
                break

        all_keys_set = True
        if not lt_key_fill:
            for fld, finfo in table.fields.iteritems():
                if finfo["TAG"] in ["key", "key_mask"] and \
                   fld not in fsets:
                    all_keys_set = False
                    continue

        if idxfield is None and \
           not all_keys_set:
            self.err_msg("ERROR: Either \"_INDEX\" field or all key/key_mask fields of the table need to be set\n")
            return None, None
        elif idxfield is not None:
            idx = fsets[idxfield]
            if not table.index_valid(idx):
                self.err_msg("ERROR: Value for \"_INDEX\" field is invalid\n")
                return None, None
        else:
            idx = table.depth
        return idx, fsets

    def __pt_op_set(self, *args):
        """
        Perform table insert operation.
        """
        typ = 0
        tbl = args[0][:-1]

        name, sid, info = self.tdb.table_get(tbl)
        if name is None:
            self.err_msg("ERROR: Unsupported physical table/register: %s\n" % (tbl))
            return
        ptype = "TABLE" if "TABLE_TYPE" in info else "REGISTER"
        if ptype == "TABLE":

            if len(args) < 3:
                self.err_msg("ERROR: Missing arguments\n")
                return self.cmd_result_set("usage")

            if not args[0].endswith(("m")):
                self.err_msg("ERROR: {} is not a physical table.\n".format(args[0]))
                return

            try:
                fname, val_str = args[2].split("=", 1)
            except ValueError:
                self.err_msg("ERROR: Invalid PT index field: %s; expecting BCMLT_PT_INDEX=<index>\n" % (args[2]))
                return
            if fname != "BCMLT_PT_INDEX":
                self.err_msg("ERROR: Invalid PT index field: %s; expecting BCMLT_PT_INDEX=<index>\n" % (args[2]))
                return
            try:
                val_str = int(val_str, 0)
            except ValueError:
                self.err_msg("ERROR: Invalid PT index field: %s; expecting BCMLT_PT_INDEX=<index>\n" % (args[2]))
                return

            idx = "_INDEX={}".format(val_str)
            nargs = (idx, ) + args[3:]
        else:

            if not args[0].endswith(("r")):
                self.err_msg("ERROR: {} is not a physical register.\n".format(args[0]))
                return

            nargs = args[2:]

        table = Table(name, **info)

        idx, fsets = self.__lt_op_parse_field(table, *nargs)
        if fsets is None:
            return
        self.__lt_op_cmd_show(name, *args)
        try:
            val, _ = table.set(**fsets)
        except ValueError as fname:
            self.err_msg("ERROR: Value for \"%s\" field is invalid\n" % (fname))
        else:
            try:
                status = self.__switch_p2l_set(sid, typ, idx, val, table.size)
            except RuntimeError as err:
                self.err_msg("%s\n" % (err))
            else:
                self.print_status(name, typ, status)

    def __pt_op_get(self, *args):
        """
        Perform table get operation.
        """
        typ = 3
        tbl = args[0][:-1]

        name, sid, info = self.tdb.table_get(tbl)
        if name is None:
            self.err_msg("ERROR: Unsupported physical table/register: %s\n" % (tbl))
            return

        ptype = "TABLE" if "TABLE_TYPE" in info else "REGISTER"
        if ptype == "TABLE":

            if len(args) < 3:
                self.err_msg("ERROR: Missing arguments\n")
                return self.cmd_result_set("usage")

            if not args[0].endswith(("m")):
                self.err_msg("ERROR: {} is not a physical table.\n".format(args[0]))
                return

            try:
                fname, val_str = args[2].split("=", 1)
            except ValueError:
                self.err_msg("ERROR: Invalid PT index field: %s; expecting BCMLT_PT_INDEX=<index>\n" % (args[2]))
                return
            if fname != "BCMLT_PT_INDEX" or \
               not val_str.isdigit():
                self.err_msg("ERROR: Invalid PT index field: %s; expecting BCMLT_PT_INDEX=<index>\n" % (args[2]))
                return
            idx = "_INDEX={}".format(val_str)
            nargs = (idx, ) + args[3:]
        else:

            if not args[0].endswith(("r")):
                self.err_msg("ERROR: {} is not a physical register.\n".format(args[0]))
                return

            nargs = args[2:]

        table = Table(name, **info)
        idx, fsets = self.__lt_op_parse_field(table, *nargs)
        if fsets is None:
            return

        self.__lt_op_cmd_show(name, *args)
        try:
            val, _ = table.set(**fsets)
        except ValueError as fname:
            self.err_msg("ERROR: Value for \"%s\" field is invalid\n" % (fname))
        else:
            try:
                (tblval, status) = self.__switch_l2p_get(sid, typ, idx, val, table.size)
                if tblval is None:
                    return self.cmd_result_set("fail. error_status {}".format(status))
            except RuntimeError as err:
                self.err_msg("%s\n" % (err))
            else:
                self.print_status(name, typ, status)
                if status == 0:
                    fvals = table.get(idx, tblval)
                    tflds = table.fields
                    retstr = ""
                    for field in sorted(fvals):
                        tag = tflds[field]["TAG"]
                        if tag in ["valid", "entry_priority", "metadata", "key_type"]:
                            continue
                        fld_str = "    %s=%s\n" % (field, format_val(fvals[field]))
                        self.out_msg(fld_str)
                        retstr += fld_str
                    if self.noncliif:
                        return retstr

    def __lt_op_cmd_show(self, name, *args):
        """
        Display the specified lt command
        """
        self.out_msg("Table %s:\n" % (name))
        self.out_msg("  %s\n" % (" ".join(args[1:])))

    def __lt_op_insert(self, *args):
        """
        Perform table insert operation.
        """

        typ = 0
        name, sid, info = self.tdb.table_get(args[0])
        if name is None:
            self.err_msg("ERROR: Unsupported logical table/register: %s\n" % (args[0]))
            return

        table = Table(name, **info)
        idx, fsets = self.__lt_op_parse_field(table, *args[2:])
        if fsets is None:
            return

        if args[2] == "-a":
            args = args[:2] + args[3:]
        self.__lt_op_cmd_show(name, *args)
        try:
            val, _ = table.set(**fsets)
        except ValueError as fname:
            self.err_msg("ERROR: Value for \"%s\" field is invalid\n" % (fname))
        else:
            try:
                status = self.__switch_p2l_set(sid, typ, idx, val, table.size)
            except RuntimeError as err:
                self.err_msg("%s\n" % (err))
            else:
                self.print_status(name, typ, status)

    def __lt_op_lookup(self, *args):
        """
        Perform table lookup operation.
        """
        typ = 3
        name, sid, info = self.tdb.table_get(args[0])
        if name is None:
            self.err_msg("ERROR: Unsupported logical table/register: %s\n" % (args[0]))
            return
        table = Table(name, **info)
        idx, fsets = self.__lt_op_parse_field(table, *args[2:])
        if fsets is None:
            return

        if idx == table.depth:
            for fld, fi in info["FIELDS"].iteritems():
                if fld in fsets and \
                   fi["TAG"] == "data":
                    del fsets[fld]

        self.__lt_op_cmd_show(name, *args)
        try:
            val, _ = table.set(**fsets)
        except ValueError as fname:
            self.err_msg("ERROR: Value for \"%s\" field is invalid\n" % (fname))
        else:
            try:
                (tblval, status) = self.__switch_l2p_get(sid, typ, idx, val, table.size)
                if tblval is None:
                    return self.cmd_result_set("fail. error_status {}".format(status))
            except RuntimeError as err:
                self.err_msg("%s\n" % (err))
            else:
                self.print_status(name, typ, status)
                if status == 0:
                    fvals = table.get(idx, tblval)
                    tflds = table.fields
                    retstr = ""
                    for field in sorted(fvals):
                        tag = tflds[field]["TAG"]
                        if tag in ["valid", "entry_priority", "metadata", "key_type"]:
                            continue
                        fld_str = "    %s=%s\n" % (field, format_val(fvals[field]))
                        self.out_msg(fld_str)
                        retstr += fld_str
                    if self.noncliif:
                        return retstr

    def __lt_op_update(self, *args):
        """
        Perform table update operation.
        """
        typ = 3
        name, sid, info = self.tdb.table_get(args[0])
        if name is None:
            self.err_msg("ERROR: Unsupported logical table/register: %s\n" % (args[0]))
            return
        table = Table(name, **info)
        idx, fsets = self.__lt_op_parse_field(table, *args[2:])
        if fsets is None:
            return
        self.__lt_op_cmd_show(name, *args)
        try:
            (tblval, status) = self.__switch_l2p_get(sid, typ, idx, 0, table.size)
            if tblval is None:
                return self.cmd_result_set("fail. error_status {}".format(status))
        except RuntimeError as err:
            self.err_msg("%s\n" % (err))
            return
        else:
            self.print_status(name, typ, status)
            if idx >= table.depth:
                # Logical table delete entry
                typ = 2
                try:
                    status = self.__switch_p2l_set(sid, typ, idx, 0, table.size)
                except RuntimeError as err:
                    self.err_msg("%s\n" % (err))
                    return
            # Insert entry
            typ = 0
            try:
                val, _ = table.update(tblval, **fsets)
            except ValueError as fname:
                self.err_msg("ERROR: Value for \"%s\" field is invalid\n" \
                         % (fname))
                return
            else:
                try:
                    status = self.__switch_p2l_set(sid, typ, idx, val, table.size)
                except RuntimeError as err:
                    self.err_msg("%s\n" % (err))
                else:
                    self.print_status(name, typ, status)

    def __lt_op_delete(self, *args):
        """
        Perform table delete operation.
        """
        typ = 2
        name, sid, info = self.tdb.table_get(args[0])
        if name is None:
            self.err_msg("ERROR: Unsupported logical table/register: %s\n" % (args[0]))
            return
        table = Table(name, **info)
        idx, fsets = self.__lt_op_parse_field(table, *args[2:])
        if fsets is None:
            return

        if idx == table.depth:
            for fld, fi in info["FIELDS"].iteritems():
                if fld in fsets and \
                   fi["TAG"] == "data":
                    del fsets[fld]

        self.__lt_op_cmd_show(name, *args)
        try:
            val, _ = table.set(**fsets)
        except ValueError as fname:
            self.err_msg("ERROR: Value for \"%s\" field is invalid\n" % (fname))
        else:
            try:
                status = self.__switch_p2l_set(sid, typ, idx, val, table.size)
            except RuntimeError as err:
                self.err_msg("%s\n" % (err))
            else:
                self.print_status(name, typ, status)

    def __pt_operation(self, *args):
        """
        Dispatch table operations.
        """

        tblops = {"set": self.__pt_op_set,
                  "get": self.__pt_op_get}
        if len(args) < 2 or args[1] not in tblops:
            return self.cmd_result_set("usage")
        tblop_fun = tblops[args[1]]
        return tblop_fun(*args)

    def __lt_operation(self, *args):
        """
        Dispatch table operations.
        """
        tblops = {"insert": self.__lt_op_insert,
                  "lookup": self.__lt_op_lookup,
                  "update": self.__lt_op_update,
                  "delete": self.__lt_op_delete}
        if len(args) < 2 or args[1] not in tblops:
            return self.cmd_result_set("usage")
        tblop_fun = tblops[args[1]]
        return tblop_fun(*args)

    def complete_lt(self, text, line, dummy_1, dummy_2):
        """
        Auto-completion for lt command.
        """
        tokens = line.split()
        num_tokens = len(tokens)
        if num_tokens < 2:
            return []
        if tokens[1] == "list":
            # Completion for lt list
            if text == "":
                # Match for all tables
                return self.tdb.tables_search("*")
            match_str = text
            if match_str in ("^", "@"):
                match_str = match_str[1:]
            return self.tdb.tables_search("^" + match_str)
        # Completion for table operations
        rlc_list = []
        tbl_ops = ["insert", "lookup", "update", "delete"]
        if num_tokens == 2 and text != "":
            # Completion for table name
            rlc_list = self.tdb.tables_search("^" + text)
        else:
            tbl_name = tokens[1]
            if self.tdb.tables_search("@" + tbl_name):
                if (num_tokens == 2 and text == "") or \
                   (num_tokens == 3 and text != ""):
                    # Completion for table operations
                    rlc_list = [tbl_op for tbl_op in tbl_ops
                                if tbl_op.startswith(text.lower())]
                elif line[-1] != "=":
                    # Completion for table fields
                    for field in self.tdb.table_fields(tbl_name):
                        if field.lower().startswith(text.lower()):
                            rlc_list.append(field)
        return rlc_list

    def complete_pt(self, text, line, dummy_1, dummy_2):
        """
        Auto-completion for pt command.
        """
        tokens = line.split()
        num_tokens = len(tokens)
        if num_tokens < 2:
            return []
        if tokens[1] == "list":
            # Completion for lt list
            if text == "":
                # Match for all tables
                return self.tdb.tables_search("*")
            match_str = text
            if match_str in ("^", "@"):
                match_str = match_str[1:]
            return self.tdb.tables_search("^" + match_str)
        # Completion for table operations
        rlc_list = []
        tbl_ops = ["get", "set"]
        if num_tokens == 2 and text != "":
            # Completion for table name
            rlc_list = self.tdb.tables_search("^" + text)
        else:
            tbl_name = tokens[1]
            if self.tdb.tables_search("@" + tbl_name):
                if (num_tokens == 2 and text == "") or \
                   (num_tokens == 3 and text != ""):
                    # Completion for table operations
                    rlc_list = [tbl_op for tbl_op in tbl_ops
                                if tbl_op.startswith(text.lower())]
                elif line[-1] != "=":
                    # Completion for table fields
                    for field in self.tdb.table_fields(tbl_name):
                        if field.lower().startswith(text.lower()):
                            rlc_list.append(field)
        return rlc_list

    def __pt_list(self, *args):
        """
        List specified table information
        """
        num_args = len(args)
        list_format = None
        idx = 0
        # Check for options
        options = {"-l":"long", "-b":"brief"}
        while idx < num_args and args[idx][0] == "-":
            if args[idx] in options:
                list_format = options[args[idx]]
            else:
                self.err_msg("ERROR: Option %s is not supported.\n" % (args[idx]))
                return
            idx += 1

        list_tables = []
        if idx == num_args:
            # Get all tables if nothing is specified.
            list_tables = self.tdb.tables_search("*", "regfile")
        else:
            for arg in args[idx:]:
                list_tables += self.tdb.tables_search(arg, "regfile")

        if len(list_tables) == 0:
            self.out_msg("No matching physical tables.\n")
            return
        for table in list_tables:
            self.out_msg("%s" % (self.tdb.table_info_get(table, list_format)))

    def tx_pkt(self, port, pkt, pkt_len):
        """
        Transmit packet
        """
        return self.__switch_tx_pkt(port, pkt, pkt_len)

    def rx_pkt(self, port=0, pkt=0, pkt_len=0):
        """
        Receive packet
        """
        return self.__switch_rx_pkt(port, pkt, pkt_len)

    def tx_pkt_hdr_info_get(self):
        """
        Get transmitted (TX) header info
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected")
            return
        return self.client.tx_pkt_hdr_info_get()

    def rx_pkt_hdr_info_get(self):
        """
        Get received (RX) header info
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected")
            return
        return self.client.rx_pkt_hdr_info_get()

    def issue_pkt_cmd(self, pkt):
        """
        Transmitted (TX) packet
        """
        if self.client is None:
            self.err_msg("ERROR: Switch is not connected")
            return
        return self.client.issue_pkt_cmd(pkt)

    def do_pt(self, dummy):
        """
        Physical table access

        pt list [-l|-b] [<name> ...]
        pt <table> get|set BCMLT_PT_INDEX=<index> <field>=<val> [<field>=<val> ...]
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        num_args = len(args)
        if num_args < 1:
            return self.cmd_result_set("usage")

        if args[0] == "list":
            return self.__pt_list(*args[1:])
        else:
            return self.__pt_operation(*args)

    def do_lt(self, dummy):
        """
        Logical table access

        lt list [-l|-b] [<name> ...]
        lt <table> insert|lookup|update|delete <field>=<val> [<field>=<val> ...]
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        num_args = len(args)
        if num_args < 1:
            return self.cmd_result_set("usage")

        if args[0] == "list":
            return self.__lt_list(*args[1:])
        else:
            return self.__lt_operation(*args)

    def do_tx(self, dummy):
        """
        Transmit packet to switch

        tx <pkt_count> pl=<port_list> data=0x<raw_pkt_data>
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        num_args = len(args)
        if num_args < 3 or args[1][:3] != "pl=" or args[2][:7] != "data=0x":
            return self.cmd_result_set("usage")

        try:
            pkt_count = int(args[0])
        except:
            self.out_msg("Invalid packet count: {}\n".format(args[0]))
            return
        try:
            port_list = []
            expr_list = args[1][3:].split(",")
            for expr in expr_list:
                range_pair = expr.split("-")
                if len(range_pair) == 1:
                    port_list.append(int(range_pair[0]))
                elif len(range_pair) == 2:
                    port_list.extend(range(int(range_pair[0]), int(range_pair[1]) +1))
                else:
                    raise ValueError("Invalid port list")
        except:
            self.out_msg("Invalid port list: {}\n".format(args[1][3:]))
            return
        pkt_bytes = args[2][7:].decode("hex")

        for i in range(pkt_count):
            for port in port_list:
                recv_pkt = self.__switch_tx_pkt(port, pkt_bytes, len(pkt_bytes))
                if recv_pkt["STATUS"]:
                    self.out_msg("Packet({}) ingress port {}. Packet is Dropped!\n".format(i, port))
                    continue

                pnum = recv_pkt["PORT"]
                pdata = binascii.hexlify(recv_pkt["PACKET"])
                self.out_msg("Packet({}) ingress port {}. Packet egress port {} data 0x{}\n".format(i, port, pnum, pdata))

                while 1:
                    recv_pkt = self.__switch_rx_pkt()

                    if recv_pkt["PORT"] == -1:
                        # No more packets
                        break
                    if recv_pkt["STATUS"]:
                        self.out_msg("Packet({}) ingress port {}. Packet is Dropped!\n".format(i, port))
                    else:
                        pnum = recv_pkt["PORT"]
                        pdata = binascii.hexlify(recv_pkt["PACKET"])
                        self.out_msg("Packet({}) ingress port {}. Packet egress port {} data 0x{}\n".format(i, port, pnum, pdata))

    def do_ctr(self, dummy):
        """
        Counter access

        ctr s [z|nz] [<table> ...]
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        num_args = len(args)
        if not (num_args >= 2 and args[0] == "s" and (args[1] == "z" or args[1] == "nz")):
            return self.cmd_result_set("usage")

        typ = 3
        list_tables = []

        # Get all counter tables
        if num_args == 2:
            for table in self.tdb.tables_search("*"):
                if re.search("COUNTER_TABLE(?:_\d+)?$", table):
                    list_tables.append(table + "m")
        else:
            list_tables.extend(args[2:])

        for counter in list_tables:
            tbl = counter[:-1]
            name, sid, info = self.tdb.table_get(tbl)
            if name is None:
                self.err_msg("ERROR: Unsupported physical table/register: %s\n" % (tbl))
                return
            table = Table(name, **info)

            maxidx = 0
            ptype = "REGISTER"
            if "TABLE_TYPE" in info:
                maxidx = info["MAXIDX"]
                ptype = "TABLE"

            # Poll all indices
            for index in xrange(maxidx + 1):
                if counter.endswith("m"):
                    if ptype != "TABLE":
                        self.err_msg("ERROR: {} is not a physical table.\n".format(counter))
                        return
                elif counter.endswith("r"):
                    if ptype != "REGISTER":
                        self.err_msg("ERROR: {} is not a physical register.\n".format(counter))
                        return
                else:
                    self.err_msg("ERROR: {} is not a physical table.\n".format(counter))
                    return

                nargs = ()
                if ptype == "TABLE":
                    idx = "_INDEX={}".format(index)
                    nargs = (idx, )
                idx, fsets = self.__lt_op_parse_field(table, *nargs)
                val, _ = table.set(**fsets)

                try:
                    (tblval, status) = self.__switch_l2p_get(sid, typ, idx, val, table.size)
                    if tblval is None:
                        return self.cmd_result_set("fail. error_status {}".format(status))
                except RuntimeError as err:
                    self.err_msg("%s\n" % (err))
                else:
                    self.print_status(name, typ, status)
                    if status == 0:
                        fvals = table.get(idx, tblval)
                        ctrvals = {}

                        # Populate counter values to be printed
                        for field in sorted(fvals):
                            if "COUNT" not in field:
                                continue
                            ctrval = format_val(fvals[field])
                            if (ctrval == "0") == (args[1] == "z"):
                                ctrvals[field] = ctrval

                        if len(ctrvals) == 0:
                            continue
                        self.__lt_op_cmd_show(name, name, "get", "BCMLT_PT_INDEX=" + str(index))
                        for field, ctrval in sorted(ctrvals.iteritems()):
                            self.out_msg("    %s=%s\n" % (field, ctrval))

    def do_crc_gen_en(self, dummy):
        """
        Enable CRC regeneration

        crc_gen_en <en>

        <en>: 0-disable; 1-enable
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) < 1:
            return self.cmd_result_set("usage")
        try:
            enable = int(args[0])
        except ValueError:
            return self.cmd_result_set("usage")
        else:
            self.__switch_crc_gen_en(enable != 0)

    def do_pkt_has_crc(self, dummy):
        """
        Indicate ingress packet contains CRC

        pkt_has_crc <crc>

        <crc>: 0-false; 1-true
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) < 1:
            return self.cmd_result_set("usage")
        try:
            crc = int(args[0])
        except ValueError:
            return self.cmd_result_set("usage")
        else:
            self.__switch_pkt_has_crc(crc != 0)

    def do_print_stats(self, dummy):
        """
        Instruct bmodel to print port statistics

        print_stats

        """
        self.__switch_print_stats(0)

    def do_disp_pkt(self, dummy):
        """
        Indicate if bmodel displays packets on console

        disp_pkt <disp>

        <disp>: 0-don"t display packet; 1-display packet
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) < 1:
            return self.cmd_result_set("usage")
        try:
            disp = int(args[0])
        except ValueError:
            return self.cmd_result_set("usage")
        else:
            self.__switch_disp_pkt(disp != 0)

    def do_debug_level(self, dummy):
        """
        Set/get the debug level

        debug_level
        debug_level <level>

        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) >= 1:
            try:
                level = int(args[0])
            except ValueError:
                return self.cmd_result_set("usage")
            self.__switch_dbg_lvl_set(level)

        lvl = self.__switch_dbg_lvl_get()
        if lvl != None:
            self.out_msg("Debug Level: %s\n" % (str(lvl)))

        if self.noncliif:
            return str(lvl)

    def do_quit(self, dummy):
        """
        Quit shell

        quit [-s]

        The server will be terminated if option "-s" is specified.
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) > 0 and args[0] == "-s":
            self.__switch_exit()
        Cli.do_quit(self, dummy)

    def do_exit(self, dummy):
        """
        Quit shell

        exit [-s]

        The server will be terminated if option "-s" is specified.
        """
        args = self.cmd_args()
        if isinstance(dummy, list):
            args = dummy
        if len(args) > 0 and args[0] == "-s":
            self.__switch_exit()
        Cli.do_exit(self, dummy)

    def tables_get_from_yml(self, yml_file):
        """
        Load table information from register file in YAML format.
        """
        if not os.path.isfile(yml_file):
            self.err_msg("ERROR: File %s is not found\n" % (yml_file))
            return
        try:
            with open(yml_file, "r") as fdesc:
                try:
                    info = yaml.load(fdesc, Loader=Loader)
                except yaml.YAMLError as err:
                    self.err_msg("ERROR: Fail to load %s from yaml\n" % (yml_file))
                    self.err_msg("%s\n" % (err))
                else:
                    for table in ["memories"]:
                        self.tdb.tables_add(**info[table])
                    for register in ["registers"]:
                        self.tdb.registers_add(**info[register])

        except IOError as err:
            self.err_msg("%s\n" % (err))
            return

    def switch_connect(self, host="localhost", port=9090):
        """
        Connect to BM server.
        """

        print("connecting to host({}), port({})".format(host, port))
        mgmt_intf = bm_mgmt_intf.bm_mgmt_intf(host, port)

        if mgmt_intf.client_socket == None:
            time.sleep(5)
            sys.exit()

        client = Switch.Client(mgmt_intf)
        self.client = client

    def switch_disconnect(self):
        """
        Disconnect from BM server.
        """
        if self.transport is not None:
            self.transport.close()

    def switch_is_connected(self):
        """
        Switch is connected
        """
        if self.client == None:
            return False

        return True


def show_help():
    """
    Display help information.
    """
    print("\nUsage:")
    print("\n  python %s --regfile <regfile.yml> " \
          "[--host <hostname>] [--port <port>]\n" % (sys.argv[0]))


def main(argv):
    """
    Entry function to run bmif_cli module.
    """
    regfile = None
    rcfile = None
    connect = {}
    try:
        opts, _ = getopt.getopt(argv, "hr:", ["regfile=", "host=", "port=",
                                              "rcfile="])
    except getopt.GetoptError:
        show_help()
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            show_help()
            sys.exit()
        elif opt in ("-r", "--regfile"):
            regfile = arg
        elif opt == "--host":
            connect["host"] = arg
        elif opt == "--port":
            try:
                port = int(arg, 0)
            except ValueError:
                print("ERROR: Invalid port number %s" % (arg))
                sys.exit(2)
            connect["port"] = port
        elif opt == "--rcfile":
            rcfile = arg

    if regfile is None:
        show_help()
        sys.exit(2)

    bmcli = BmifCli(prompt="bmcli.0> ")
    print("Reading regfile from {} ...".format(regfile))
    bmcli.tables_get_from_yml(regfile)
    bmcli.switch_connect(**connect)
    if bmcli.transport is None:
        bmcli.prompt = "bmcli> "
    if rcfile is not None:
        print("Reading rcfile from {} ...".format(rcfile))
        bmcli.onecmd(bmcli.precmd("rcload {}" . format(rcfile)))
    bmcli.cmdloop()
    bmcli.switch_disconnect()


if __name__ == "__main__":
    main(sys.argv[1:])
