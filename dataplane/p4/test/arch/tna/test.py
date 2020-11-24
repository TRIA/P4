#################################################################################################
# BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
#
# Copyright (c) 2019-present Barefoot Networks, Inc.
#
# All Rights Reserved.
#
# NOTICE: All information contained herein is, and remains the property of
# Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
# technical concepts contained herein are proprietary to Barefoot Networks, Inc.
# and its suppliers and may be covered by U.S. and Foreign Patents, patents in
# process, and are protected by trade secret or copyright law.  Dissemination of
# this information or reproduction of this material is strictly forbidden unless
# prior written permission is obtained from Barefoot Networks, Inc.
#
# No warranty, explicit or implicit is provided, unless granted under a written
# agreement with Barefoot Networks, Inc.
#
################################################################################

import logging
import random

from ptf import config
from collections import namedtuple
import ptf.testutils as testutils
from bfruntime_client_base_tests import BfRuntimeTest
import bfrt_grpc.client as gc
import grpc

import unittest
from scapy.all import bind_layers, Dot1Q, Ether, hexdump, IP, TCP, Raw
from edf_scapy_models import EFCP
import codecs
from edf_pdu_types import EFCP_TYPES

# ScaPy initialisation
bind_layers(Ether, Dot1Q, type=0x8100)
## EFCP (untagged and tagged)
bind_layers(Ether, EFCP, type=0xD1F)
bind_layers(Dot1Q, EFCP, type=0xD1F)
## IPv4 (untagged and tagged)
bind_layers(Ether, IP, type=0x0800)
bind_layers(Dot1Q, IP, type=0x0800)

logger = logging.getLogger("Test")
if not len(logger.handlers):
    logger.addHandler(logging.StreamHandler())

swports = []
for device, port, ifname in config["interfaces"]:
    swports.append(port)
    swports.sort()

if swports == []:
    swports = list(range(9))


class SingleRandomSeed(object):
    """
    Unique class (using singleton) to ensure the same
    random seed and num_entries are in use for all classes.
    """
    __instance = None

    def __new__(self, *args, **kwargs):
        if SingleRandomSeed.__instance is None:
            SingleRandomSeed.__instance = object.__new__(self)
            # Variables defined only during the (first and only) instantiation
            SingleRandomSeed.__instance.seed = random.randint(1, 65535)
            logger.info("Seed used %d", SingleRandomSeed.__instance.seed)
            random.seed(SingleRandomSeed.__instance.seed)
            # Note: values must be passed as kwargs
            if "MAX_PKTS_SENT" not in kwargs.keys():
                raise Exception("SingleRandomSeed missing initialisation variables")
            for k, v in kwargs.items():
                setattr(SingleRandomSeed.__instance, k, v)
            SingleRandomSeed.__instance.num_entries = random.randint(1, 
                    SingleRandomSeed.__instance.MAX_PKTS_SENT)
            logger.info("Number of entries to be tested %d", 
                    SingleRandomSeed.__instance.num_entries)
        return SingleRandomSeed.__instance


class BaseEDFTest(BfRuntimeTest):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.MINSIZE = 0
        self.MAX_PKTS_SENT = 15
        self.counter_test = False
        self.counter_table = None
        self.single_rand = SingleRandomSeed(MAX_PKTS_SENT=self.MAX_PKTS_SENT)
        self.seed = self.single_rand.seed
        self.num_entries = self.single_rand.num_entries

    def setUp(self):
        client_id = 0
        p4_name = "tna_efcp"
        BfRuntimeTest.setUp(self, client_id, p4_name)

    def add_dot1q_header(self, pkt, dl_vlanid_list, dl_vlan_pcp_list, dl_vlan_cfi_list, dl_tpid_list):
        # Note Dot1Q.id is really DEI (aka CFI)
        for i in range(0, len(dl_vlanid_list)):
            dot1q_pkt = Dot1Q(prio=dl_vlan_pcp_list[i], id=dl_vlan_cfi_list[i], vlan=dl_vlanid_list[i])
            pkt = pkt / dot1q_pkt
        for i in range(1, len(dl_tpid_list)):
            pkt[Dot1Q:i].type=dl_tpid_list[i]
        pkt.type=dl_tpid_list[0]
        return pkt

    def insert_counter_entries(self, target, gc, eg_port):
        if self.counter_test:
            # Add new counter
            self.counter_table.entry_add(target,
                    [self.counter_table.make_key(
                        [gc.KeyTuple("$COUNTER_INDEX", eg_port)])
                    ],
                    [self.counter_table.make_data(
                        [gc.DataTuple("$COUNTER_SPEC_BYTES", 0),
                        gc.DataTuple("$COUNTER_SPEC_PKTS", 0)])
                    ])
            # Default packet size is 100 bytes and model adds 4 bytes of CRC
            pkt_size = 100 + 4
            num_pkts = self.num_entries
            num_bytes = num_pkts * pkt_size

    def check_entries_have_proper_keys(self, table, target, table_dict):
        # Check get
        resp  = table.entry_get(target)
        for data, key in resp:
            lpm_dict_keys = [ _ for _ in table_dict.keys() ]
            assert table_dict[key] == data
            table_dict.pop(key)
        assert len(table_dict) == 0

    def send_packet(self, ig_port, pkt, exp_pkt, target, type_log,
            gc, vlan_enable, vlan_id, key_item, data_item):
        vlan_log_info = ""
        if vlan_enable:
            vlan_log_info = ", vlan_id %s" % vlan_id
        logger.info("Sending packet on port %d%s", ig_port, vlan_log_info)
        testutils.send_packet(self, ig_port, pkt)

        logger.info("Verifying entry for %s%s" % (type_log, vlan_log_info))
        logger.info("Expecting packet on port %d%s", data_item.eg_port, vlan_log_info)
        try:
            testutils.verify_packets(self, exp_pkt, [data_item.eg_port])
        except Exception as e:
            logger.error("Error on packet verification")
            raise e

        if self.counter_test:
            resp = self.counter_table.entry_get(target,[self.counter_table.make_key([gc.KeyTuple("$COUNTER_INDEX", data_item.eg_port)])], {"from_hw": True}, None)
            # parse resp to get the counter
            data_dict = next(resp)[0].to_dict()
            recv_pkts = data_dict["$COUNTER_SPEC_PKTS"]
            recv_bytes = data_dict["$COUNTER_SPEC_BYTES"]
            logger.info("The counter value for port %s is %s", data_item.eg_port, str(recv_pkts))

    def delete_rules(self, target, id_list, table, key_lambda):
        # Delete table entries
        for i in range(0, len(id_list)):
            table.entry_del(
                target,
                [table.make_key(key_lambda(i))])


class IPV4Test(BaseEDFTest):

    # EFCP packet submission adapted from the method with a similar name (for TCP)
    # under $SDE_INSTALL/lib/python3.6/site-packages/ptf/testutils.py
    def simple_tcp_packet_ext_taglist(self,
                                  pkt=None,
                                  pktlen=100,
                                  ip_dst="10.10.10.10",
                                  dl_taglist_enable=False,
                                  dl_vlan_pcp_list=[0],
                                  dl_vlan_cfi_list=[0],
                                  dl_tpid_list=[0x8100],
                                  dl_vlanid_list=[1]
                                  ):

        if self.MINSIZE > pktlen:
            pktlen = self.MINSIZE
        if not pkt:
            pkt = testutils.simple_tcp_packet(ip_dst=ip_dst)
        eth_pkt = pkt[Ether]
        # IP packet contains IP, TCP, Raw headers
        ip_pkt = pkt[IP]

        pkt = Ether(dst = eth_pkt.dst, src = eth_pkt.src, type = eth_pkt.type)

        dl_taglist_enable = True
        if dl_taglist_enable:
            pkt = self.add_dot1q_header(pkt, dl_vlanid_list, dl_vlan_pcp_list, dl_vlan_cfi_list, dl_tpid_list)

        pkt = pkt / ip_pkt
        return pkt
    
    def simple_tcp_packet(self,
                      pkt=None,
                      pktlen=100,
                      ip_dst="10.10.10.10",
                      dl_vlan_enable=False,
                      vlan_vid=0,
                      vlan_pcp=0,
                      dl_vlan_cfi=0
                      ):
        pcp_list = []
        cfi_list = []
        tpid_list = []
        vlan_list = []

        if (dl_vlan_enable):
            pcp_list.append(vlan_pcp)
            cfi_list.append(dl_vlan_cfi)
            tpid_list.append(0x8100)
            vlan_list.append(vlan_vid)
        pkt = self.simple_tcp_packet_ext_taglist(pktlen=pktlen,
                                        ip_dst=ip_dst,
                                        dl_taglist_enable=dl_vlan_enable,
                                        dl_vlan_pcp_list=pcp_list,
                                        dl_vlan_cfi_list=cfi_list,
                                        dl_tpid_list=tpid_list,
                                        dl_vlanid_list=vlan_list)
        return pkt

    def _run_test(self, *args, **kwargs):
        try:
            self.counter_test = kwargs["counter_test"]
        except Exception as e:
            logger.error("Could not load \"counter_test\" flag - defaults to False. Details: {0}".format(e))
        ig_port = swports[1]

        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get("tna_efcp")
        if self.counter_test:
            self.counter_table = bfrt_info.table_get("SwitchIngress.ipv4_counter")        
        ipv4_lpm_table = bfrt_info.table_get("SwitchIngress.ipv4_lpm")
        ipv4_lpm_table.info.key_field_annotation_add("hdr.ipv4.dst_addr", "ipv4")
        ipv4_lpm_table.info.data_field_annotation_add("vlan_id", "SwitchIngress.ipv4_forward", "bit")
        ipv4_lpm_table.info.data_field_annotation_add("src_mac", "SwitchIngress.ipv4_forward", "mac")
        ipv4_lpm_table.info.data_field_annotation_add("dst_mac", "SwitchIngress.ipv4_forward", "mac")

        key_random_tuple = namedtuple("key_random", "vrf dst_ip prefix_len")
        data_random_tuple = namedtuple("data_random", "vlan_id smac dmac eg_port")
        key_tuple_list = []
        data_tuple_list = []
        unique_keys = {}
        lpm_dict= {}

        logger.info("Installing %d ALPM entries" % (self.num_entries))
        ip_list = self.generate_random_ip_list(self.num_entries, self.seed)
        vlan_id_list = [random.randint(0, 4095) for x in range(len(ip_list))]

        for i in range(0, len(ip_list)):
            vrf = 0
            dst_ip = getattr(ip_list[i], "ip")
            p_len = getattr(ip_list[i], "prefix_len")

            src_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            dst_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            eg_port = swports[random.randint(1, 4)]
            vlan_id = vlan_id_list[i]

            key_tuple_list.append(key_random_tuple(vrf, dst_ip, p_len))
            data_tuple_list.append(data_random_tuple(vlan_id, src_mac, dst_mac, eg_port))

            target = gc.Target(device_id=0, pipe_id=0xffff)
            logger.info("Inserting table entry with IP address %s, prefix length %d" % (dst_ip, p_len))
            logger.info("With expected vlan_id %s, dst_mac %s, src_mac %s on port %d" % (vlan_id, src_mac, dst_mac, eg_port))
            key = ipv4_lpm_table.make_key([gc.KeyTuple("hdr.ipv4.dst_addr", dst_ip, prefix_len=p_len)])
            data = ipv4_lpm_table.make_data([gc.DataTuple("vlan_id", vlan_id),
                                               gc.DataTuple("src_mac", src_mac),
                                               gc.DataTuple("dst_port", eg_port),
                                               gc.DataTuple("dst_mac", dst_mac)],
                                              "SwitchIngress.ipv4_forward")
            ipv4_lpm_table.entry_add(target, [key], [data])
            key.apply_mask()
            lpm_dict[key] = data

            self.insert_counter_entries(target, gc, eg_port)

        self.check_entries_have_proper_keys(ipv4_lpm_table, target, lpm_dict)

        test_tuple_list = list(zip(key_tuple_list, data_tuple_list))

        logger.info("Sending packets for the installed entries to verify")
        # send pkt and verify sent
        i = 0
        for key_item, data_item in test_tuple_list:
            vlan_id = vlan_id_list[i]
            vlan_enable = i % 2 == 0
            i += 1

            #tcp_pkt = testutils.simple_tcp_packet(ip_dst=key_item.dst_ip)
            #pkt = self.simple_tcp_packet(pkt=tcp_pkt, ip_dst=key_item.dst_ip, \
            #    dl_vlan_enable=vlan_enable, vlan_vid=vlan_id, \
            #)
            #exp_pkt = self.simple_tcp_packet(pkt=tcp_pkt, ip_dst=key_item.dst_ip, \
            #    dl_vlan_enable=vlan_enable, vlan_vid=vlan_id, \
            #)
            
            #logger.info("ScaPy emitted packet")
            pkt = testutils.simple_tcp_packet(ip_dst=key_item.dst_ip, \
                    dl_vlan_enable=vlan_enable, vlan_vid=vlan_id)
            #pkt.show()

            #logger.info("ScaPy expected packet")
            exp_pkt = testutils.simple_tcp_packet(eth_dst=data_item.dmac,
                                                  eth_src=data_item.smac,
                                                  ip_dst=key_item.dst_ip,
                                                  dl_vlan_enable=vlan_enable, vlan_vid=vlan_id)
            #exp_pkt.show()

            type_log = "Verifying entry for %s IP address, prefix_length %d" % \
                (key_item.dst_ip, key_item.prefix_len)
            self.send_packet(ig_port, pkt, exp_pkt, target, type_log,
                    gc, vlan_enable, vlan_id, key_item, data_item)

        logger.info("All expected packets received")
        logger.info("Deleting %d ALPM entries" % (self.num_entries))
        key_lambda = lambda idx: [gc.KeyTuple("hdr.ipv4.dst_addr", key_tuple_list[idx].dst_ip,
            prefix_len=key_tuple_list[idx].prefix_len)]
        self.delete_rules(target, key_tuple_list, ipv4_lpm_table, key_lambda)
        # Whilst syntactically correct, this is a non-supported operations: counter tables are always there
        #if self.counter_test:
        #    logger.info("Deleting %d counter entries" % (self.num_entries))
        #    key_lambda = lambda idx: [gc.KeyTuple("$COUNTER_INDEX", data_tuple_list[idx].eg_port)]
        #    self.delete_rules(target, data_tuple_list, self.counter_table, key_lambda)


#@unittest.skip("Filtering")
class IPV4LpmMatchTest(IPV4Test):
    """
    @brief Basic test for IPV4 lpm match.
    """

    def runTest(self):
        test_options = {
            "counter_test": False
        }
        self._run_test(**test_options)


#@unittest.skip("Filtering")
class IPv4IndirectCounterTest(IPV4Test):
    """
    @brief Basic test for IPV4 counter test.
    """

    def runTest(self):
        test_options = {
            "counter_test": True
        }
        self._run_test(**test_options)


class EFCPTest(BaseEDFTest):
    """
    @brief Base EFCP test.
    """

    # EFCP packet submission adapted from the method with a similar name (for TCP)
    # under $SDE_INSTALL/lib/python3.6/site-packages/ptf/testutils.py
    def simple_efcp_packet_ext_taglist(self,
                                  pktlen=100,
                                  eth_dst="00:01:02:03:04:05",
                                  eth_src="00:06:07:08:09:0a",
                                  dl_taglist_enable=False,
                                  dl_vlan_pcp_list=[0],
                                  dl_vlan_cfi_list=[0],
                                  dl_tpid_list=[0x8100],
                                  dl_vlanid_list=[1],
                                  ipc_src_addr=1,
                                  ipc_dst_addr=2
                                  ):

        if self.MINSIZE > pktlen:
            pktlen = self.MINSIZE

        efcp_payload = "EFCP packet sent from CLI to BM :)"
        efcp_pkt = EFCP(ipc_src_addr=ipc_src_addr, ipc_dst_addr=ipc_dst_addr, pdu_type=EFCP_TYPES["DATA_TRANSFER"])
        # Checksum computation is not correct anymore
        # efcp_pkt.hdr_checksum = checksum(efcp_pkt[:20])
        pkt = efcp_pkt / efcp_payload
        eth_pkt = Ether(dst=eth_dst, src=eth_src, type=0xD1F)
        pkt = eth_pkt

        if dl_taglist_enable:
            pkt = self.add_dot1q_header(pkt, dl_vlanid_list, dl_vlan_pcp_list, dl_vlan_cfi_list, dl_tpid_list)
        pkt = pkt / efcp_pkt

        codecs_decode_range = [ x for x in range(pktlen - len(pkt)) ]
        codecs_decode_str = "".join(["%02x"%(x%256) for x in range(pktlen - len(pkt)) ])
        codecs_decode = codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
        pkt = pkt / codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
        return pkt

    def simple_efcp_packet(self,
                      pktlen=100,
                      eth_dst="00:01:02:03:04:05",
                      eth_src="00:06:07:08:09:0a",
                      dl_vlan_enable=False,
                      vlan_vid=0,
                      vlan_pcp=0,
                      dl_vlan_cfi=0,
                      ipc_src_addr=1,
                      ipc_dst_addr=2,
                      ):
        pcp_list = []
        cfi_list = []
        tpid_list = []
        vlan_list = []

        if (dl_vlan_enable):
            pcp_list.append(vlan_pcp)
            cfi_list.append(dl_vlan_cfi)
            tpid_list.append(0x8100)
            vlan_list.append(vlan_vid)
        pkt = self.simple_efcp_packet_ext_taglist(pktlen=pktlen,
                                        eth_dst=eth_dst,
                                        eth_src=eth_src,
                                        dl_taglist_enable=dl_vlan_enable,
                                        dl_vlan_pcp_list=pcp_list,
                                        dl_vlan_cfi_list=cfi_list,
                                        dl_tpid_list=tpid_list,
                                        dl_vlanid_list=vlan_list,
                                        ipc_src_addr=ipc_src_addr,
                                        ipc_dst_addr=ipc_dst_addr)
        return pkt

    def _run_test(self, *args, **kwargs):
        try:
            self.counter_test = kwargs["counter_test"]
        except Exception as e:
            logger.error("Could not load \"counter_test\" flag - defaults to False. Details: {0}".format(e))
        ig_port = swports[1]

        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get("tna_efcp")
        efcp_exact_table = bfrt_info.table_get("SwitchIngress.efcp_exact")
        if self.counter_test:
            self.counter_table = bfrt_info.table_get("SwitchIngress.efcp_counter")
        efcp_exact_table.info.key_field_annotation_add("hdr.efcp.dst_addr", "bit")
        efcp_exact_table.info.data_field_annotation_add("vlan_id", "SwitchIngress.efcp_forward", "bit")
        efcp_exact_table.info.data_field_annotation_add("dst_mac", "SwitchIngress.efcp_forward", "mac")
        efcp_exact_table.info.data_field_annotation_add("dst_port", "SwitchIngress.efcp_forward", "bit")

        key_random_tuple = namedtuple("key_random", "dst_addr")
        data_random_tuple = namedtuple("data_random", "vlan_id dst_mac eg_port")
        key_tuple_list = []
        data_tuple_list = []
        unique_keys = {}
        exact_dict= {}

        efcp_id_list = list(set([random.randint(0, 255) for x in range(self.num_entries)]))
        vlan_id_list = [random.randint(0, 4095) for x in range(len(efcp_id_list))]
        logger.info("Installing %d exact entries" % (len(efcp_id_list)))

        for i in range(0, len(efcp_id_list)):
            vrf = 0
            dst_ipc = efcp_id_list[i]
            vlan_id = vlan_id_list[i]
            dst_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            eg_port = swports[random.randint(1, 4)]

            key_tuple_list.append(key_random_tuple(dst_ipc))
            data_tuple_list.append(data_random_tuple(vlan_id, dst_mac, eg_port))

            target = gc.Target(device_id=0, pipe_id=0xffff)
            logger.info("Inserting table entry with IPC ID %s" % dst_ipc)
            logger.info("With expected vlan_id %s, dst_mac %s on port %d" % (vlan_id, dst_mac, eg_port))
            key = efcp_exact_table.make_key([gc.KeyTuple("hdr.efcp.dst_addr", dst_ipc)])
            data = efcp_exact_table.make_data([gc.DataTuple("vlan_id", vlan_id),
                                               gc.DataTuple("dst_mac", dst_mac),
                                               gc.DataTuple("dst_port", eg_port)],
                                              "SwitchIngress.efcp_forward")
            efcp_exact_table.entry_add(target, [key], [data])
            key.apply_mask()
            exact_dict[key] = data

            self.insert_counter_entries(target, gc, eg_port)

        self.check_entries_have_proper_keys(efcp_exact_table, target, exact_dict)

        test_tuple_list = list(zip(key_tuple_list, data_tuple_list))
        
        logger.info("Sending packets for the installed entries to verify")

        # send pkt and verify sent
        i = 0
        for key_item, data_item in test_tuple_list:
            efcp_src_id = random.randint(0, 255)
            src_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            vlan_id = vlan_id_list[i]
            vlan_enable = i % 2 == 0
            i += 1

            #logger.info("ScaPy emitted packet")
            pkt = self.simple_efcp_packet(eth_src=src_mac, eth_dst=data_item.dst_mac, \
                    dl_vlan_enable=vlan_enable, vlan_vid=vlan_id, \
                    ipc_src_addr=efcp_src_id, ipc_dst_addr=key_item.dst_addr \
                    )
            #pkt.show()
            
            #logger.info("ScaPy expected packet")
            exp_pkt = self.simple_efcp_packet(eth_dst=data_item.dst_mac, eth_src=data_item.dst_mac, \
                    dl_vlan_enable=vlan_enable, vlan_vid=vlan_id, \
                    ipc_src_addr=efcp_src_id, ipc_dst_addr=key_item.dst_addr \
                    )
            #exp_pkt.show()

            type_log = "Verifying entry for IPC ID %s" % key_item.dst_addr
            self.send_packet(ig_port, pkt, exp_pkt, target, type_log,
                    gc, vlan_enable, vlan_id, key_item, data_item)
        
        logger.info("All expected packets received")
        logger.info("Deleting %d Exact entries" % (len(efcp_id_list)))
        key_lambda = lambda idx: [gc.KeyTuple("hdr.efcp.dst_addr", efcp_id_list[idx])]
        self.delete_rules(target, efcp_id_list, efcp_exact_table, key_lambda)
        # Whilst syntactically correct, this is a non-supported operations: counter tables are always there
        #if self.counter_test:
        #    logger.info("Deleting %d counter entries" % (self.num_entries))
        #    key_lambda = lambda idx: [gc.KeyTuple("$COUNTER_INDEX", data_tuple_list[idx].eg_port)]
        #    self.delete_rules(target, data_tuple_list, self.counter_table, key_lambda)


#@unittest.skip("Filtering")
class EFCPExactMatchTest(EFCPTest):
    """
    @brief Basic test for EFCP exact match.
    """

    def runTest(self):
        test_options = {
            "counter_test": False
        }
        self._run_test(**test_options)


#@unittest.skip("Filtering")
class EFCPCounterTest(EFCPTest):
    """
    @brief Basic test for EFCP counter test.
    """

    def runTest(self):
        test_options = {
            "counter_test": True
        }
        self._run_test(**test_options)
