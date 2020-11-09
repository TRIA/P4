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
from scapy.all import bind_layers, Dot1Q, Ether, hexdump, IP
from edf_scapy_models import EFCP, VLAN
import codecs
from edf_pdu_types import EFCP_TYPES

# ScaPy initialisation
bind_layers(Ether, EFCP, type=0xD1F)

logger = logging.getLogger("Test")
if not len(logger.handlers):
    logger.addHandler(logging.StreamHandler())

swports = []
for device, port, ifname in config["interfaces"]:
    swports.append(port)
    swports.sort()

if swports == []:
    swports = list(range(9))


@unittest.skip("Filtering")
class IPV4LpmMatchTest(BfRuntimeTest):
    """@brief Basic test for algorithmic-lpm-based lpm matches.
    """

    def setUp(self):
        client_id = 0
        p4_name = "tna_efcp_test"
        BfRuntimeTest.setUp(self, client_id, p4_name)

    def runTest(self):
        ig_port = swports[1]
        seed = random.randint(1, 65535)
        logger.info("Seed used %d", seed)
        random.seed(seed)
        num_entries = random.randint(1, 30)

        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get("tna_efcp_test")
        ipv4_lpm_table = bfrt_info.table_get("SwitchIngress.ipv4_lpm")
        ipv4_lpm_table.info.key_field_annotation_add("hdr.ipv4.dst_addr", "ipv4")
        ipv4_lpm_table.info.data_field_annotation_add("src_mac", "SwitchIngress.ipv4_forward", "mac")
        ipv4_lpm_table.info.data_field_annotation_add("dst_mac", "SwitchIngress.ipv4_forward", "mac")

        key_random_tuple = namedtuple("key_random", "vrf dst_ip prefix_len")
        data_random_tuple = namedtuple("data_random", "smac dmac eg_port")
        key_tuple_list = []
        data_tuple_list = []
        unique_keys = {}
        lpm_dict= {}

        logger.info("Installing %d ALPM entries" % (num_entries))
        ip_list = self.generate_random_ip_list(num_entries, seed)
        for i in range(0, num_entries):
            vrf = 0
            dst_ip = getattr(ip_list[i], "ip")
            p_len = getattr(ip_list[i], "prefix_len")

            src_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            dst_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            eg_port = swports[random.randint(1, 4)]

            key_tuple_list.append(key_random_tuple(vrf, dst_ip, p_len))
            data_tuple_list.append(data_random_tuple(src_mac, dst_mac, eg_port))

            target = gc.Target(device_id=0, pipe_id=0xffff)
            logger.info("Inserting table entry with IP address %s, prefix length %d" % (dst_ip, p_len))
            logger.info("With expected dst_mac %s, src_mac %s on port %d" % (src_mac, dst_mac, eg_port))
            key = ipv4_lpm_table.make_key([gc.KeyTuple("hdr.ipv4.dst_addr", dst_ip, prefix_len=p_len)])
            data = ipv4_lpm_table.make_data([gc.DataTuple("dst_port", eg_port),
                                               gc.DataTuple("src_mac", src_mac),
                                               gc.DataTuple("dst_mac", dst_mac)],
                                              "SwitchIngress.ipv4_forward")
            ipv4_lpm_table.entry_add(target, [key], [data])
            key.apply_mask()
            lpm_dict[key] = data

        # check get
        resp  = ipv4_lpm_table.entry_get(target)
        for data, key in resp:
            lpm_dict_keys = [ _ for _ in lpm_dict.keys() ]
            assert lpm_dict[key] == data
            lpm_dict.pop(key)
        assert len(lpm_dict) == 0

        test_tuple_list = list(zip(key_tuple_list, data_tuple_list))

        logger.info("Sending packets for the installed entries to verify")
        # send pkt and verify sent
        for key_item, data_item in test_tuple_list:
            pkt = testutils.simple_tcp_packet(ip_dst=key_item.dst_ip)
            exp_pkt = testutils.simple_tcp_packet(eth_dst=data_item.dmac,
                                                  eth_src=data_item.smac,
                                                  ip_dst=key_item.dst_ip)
            logger.info("Sending packet on port %d", ig_port)
            testutils.send_packet(self, ig_port, pkt)

            logger.info("Verifying entry for IP address %s, prefix_length %d" % (key_item.dst_ip, key_item.prefix_len))
            logger.info("Expecting packet on port %d", data_item.eg_port)
            testutils.verify_packets(self, exp_pkt, [data_item.eg_port])

        logger.info("All expected packets received")
        logger.info("Deleting %d ALPM entries" % (num_entries))

        # Delete table entries
        for item in key_tuple_list:
            ipv4_lpm_table.entry_del(
                target,
                [ipv4_lpm_table.make_key([gc.KeyTuple("hdr.ipv4.dst_addr", item.dst_ip,
                                                          prefix_len=item.prefix_len)])])


@unittest.skip("Filtering")
class IPv4IndirectCounterTest(BfRuntimeTest):
    """@brief Basic test for counting IPv4 packets.
    """

    def setUp(self):
        client_id = 0
        p4_name = "tna_efcp_test"
        BfRuntimeTest.setUp(self, client_id, p4_name)

    def runTest(self):
        ig_port = swports[1]
        seed = random.randint(1, 65535)
        logger.info("Seed used %d", seed)
        random.seed(seed)
        num_entries = random.randint(1, 30)

        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get("tna_efcp_test")
        ipv4_lpm_table = bfrt_info.table_get("SwitchIngress.ipv4_lpm")
        counter_table = bfrt_info.table_get("SwitchIngress.ipv4_counter")
        ipv4_lpm_table.info.key_field_annotation_add("hdr.ipv4.dst_addr", "ipv4")
        ipv4_lpm_table.info.data_field_annotation_add("src_mac", "SwitchIngress.ipv4_forward", "mac")
        ipv4_lpm_table.info.data_field_annotation_add("dst_mac", "SwitchIngress.ipv4_forward", "mac")

        key_random_tuple = namedtuple("key_random", "vrf dst_ip prefix_len")
        data_random_tuple = namedtuple("data_random", "smac dmac eg_port")
        key_tuple_list = []
        data_tuple_list = []
        unique_keys = {}
        lpm_dict= {}

        logger.info("Installing %d ALPM entries" % (num_entries))
        ip_list = self.generate_random_ip_list(num_entries, seed)
        for i in range(0, num_entries):
            vrf = 0
            dst_ip = getattr(ip_list[i], "ip")
            p_len = getattr(ip_list[i], "prefix_len")

            src_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            dst_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])
            eg_port = swports[random.randint(1, 4)]

            key_tuple_list.append(key_random_tuple(vrf, dst_ip, p_len))
            data_tuple_list.append(data_random_tuple(src_mac, dst_mac, eg_port))

            target = gc.Target(device_id=0, pipe_id=0xffff)
            logger.info("Inserting table entry with IP address %s, prefix length %d" % (dst_ip, p_len))
            logger.info("With expected dst_mac %s, src_mac %s on port %d" % (src_mac, dst_mac, eg_port))
            key = ipv4_lpm_table.make_key([gc.KeyTuple("hdr.ipv4.dst_addr", dst_ip, prefix_len=p_len)])
            data = ipv4_lpm_table.make_data([gc.DataTuple("dst_port", eg_port),
                                               gc.DataTuple("src_mac", src_mac),
                                               gc.DataTuple("dst_mac", dst_mac)],
                                              "SwitchIngress.ipv4_forward")
            ipv4_lpm_table.entry_add(target, [key], [data])
            key.apply_mask()
            lpm_dict[key] = data

            # add new counter
            counter_table.entry_add(target,[counter_table.make_key([gc.KeyTuple('$COUNTER_INDEX', eg_port)])],[counter_table.make_data([gc.DataTuple('$COUNTER_SPEC_BYTES', 0),gc.DataTuple('$COUNTER_SPEC_PKTS', 0)])])
            # default packet size is 100 bytes and model adds 4 bytes of CRC
            pkt_size = 100 + 4
            num_pkts = num_entries
            num_bytes = num_pkts * pkt_size

        # check get
        resp  = ipv4_lpm_table.entry_get(target)
        for data, key in resp:
            assert lpm_dict[key] == data
            lpm_dict.pop(key)
        assert len(lpm_dict) == 0

        test_tuple_list = list(zip(key_tuple_list, data_tuple_list))

        logger.info("Sending packets for the installed entries to verify")
        # send pkt and verify sent
        for key_item, data_item in test_tuple_list:
            pkt = testutils.simple_tcp_packet(ip_dst=key_item.dst_ip)
            exp_pkt = testutils.simple_tcp_packet(eth_dst=data_item.dmac,
                                                  eth_src=data_item.smac,
                                                  ip_dst=key_item.dst_ip)
            logger.info("Sending packet on port %d", ig_port)
            testutils.send_packet(self, ig_port, pkt)

            logger.info("Verifying entry for IP address %s, prefix_length %d" % (key_item.dst_ip, key_item.prefix_len))
            logger.info("Expecting packet on port %d", data_item.eg_port)
            testutils.verify_packets(self, exp_pkt, [data_item.eg_port])

            resp = counter_table.entry_get(target,[counter_table.make_key([gc.KeyTuple('$COUNTER_INDEX', data_item.eg_port)])],{"from_hw": True},None)

            # parse resp to get the counter
            data_dict = next(resp)[0].to_dict()
            recv_pkts = data_dict["$COUNTER_SPEC_PKTS"]
            recv_bytes = data_dict["$COUNTER_SPEC_BYTES"]

            logger.info("The counter value for port %s is %s",data_item.eg_port,str(recv_pkts))

        logger.info("All expected packets received")
        logger.info("Deleting %d ALPM entries" % (num_entries))

        # Delete table entries
        for item in key_tuple_list:
            ipv4_lpm_table.entry_del(
                target,
                [ipv4_lpm_table.make_key([gc.KeyTuple("hdr.ipv4.dst_addr", item.dst_ip,
                prefix_len=item.prefix_len)])])


#@unittest.skip("Filtering")
# NOTE: it should be no longer needed to run "bfrt.tna_efcp_test.pipe.SwitchIngress.efcp_exact.clear()" and then
# "bfrt.tna_efcp_test.pipe.SwitchIngress.efcp_exact.dump()" to verify
# there are no EFCP-related rules in the Tofino switch before running this test
class EFCPExactMatchTest(BfRuntimeTest):
    """@brief Basic test for algorithmic-lpm-based lpm matches.
    """

    # DO NOT USE. Just to check
#    def _get_tx_packet(self, ipc_dst_addr, ipc_src_addr, mac_dst_addr, mac_src_addr, vlan_id):
#        # FIXME: failing with type in Ether() - check why
#        #pkt = Ether(dst=mac_dst_addr, src=mac_src_addr, type=0xD1F) / Dot1Q(vlan=vlan_id) / EFCP(ipc_dst_addr=ipc_dst_addr, ipc_src_addr=ipc_src_addr, pdu_type=0x8001)
#        pkt = Ether(dst=mac_dst_addr, src=mac_src_addr) / Dot1Q(vlan=vlan_id) / EFCP(ipc_dst_addr=ipc_dst_addr, ipc_src_addr=ipc_src_addr, pdu_type=0x8001)
#        pkt = pkt / "EFCP packet sent from CLI to BM :)"
#        pktlen = 100
#        codecs_decode_range = [ x for x in range(pktlen - len(pkt)) ]
#        codecs_decode_str = "".join(["%02x"%(x%256) for x in range(pktlen - len(pkt)) ])
#        codecs_decode = codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
#        pkt = pkt / codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
#        return pkt

    MINSIZE = 0

    # Adapted from simple_tcp_packet_ext_taglist in $SDE_INSTALL/lib/python3.6/site-packages/ptf/testutils.py
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
        
        #bind_layers(Ether, VLAN)
        #bind_layers(VLAN, EFCP, type=0x8100)

        efcp_payload = "EFCP packet sent from CLI to BM :)"
        # FIXME: failing with type in Ether() - check why
        #efcp_pkt = Ether(dst=eth_dst, src=eth_src, type=0x8100) / VLAN(ethertype=0xD1F) / EFCP(ipc_dst_addr=ipc_dst_addr, ipc_src_addr=ipc_src_addr, pdu_type=0x8001)
        efcp_pkt = EFCP(ipc_src_addr=ipc_src_addr, ipc_dst_addr=ipc_dst_addr, pdu_type=EFCP_TYPES["DATA_TRANSFER"])
        # Computed in the expected one (may need to be used -- if so fix it)
        # NOTE: comment if this gives issues, but uncomment after fixing
        # efcp_pkt.hdr_checksum = checksum(efcp_pkt[:20])
        pkt = efcp_pkt / efcp_payload
        eth_pkt = Ether(dst=eth_dst, src=eth_src, type=0xD1F)

        pkt = eth_pkt

        # Note Dot1Q.id is really CFI
        if (dl_taglist_enable):
            for i in range(0, len(dl_vlanid_list)):
                pkt = pkt / Dot1Q(prio=dl_vlan_pcp_list[i], id=dl_vlan_cfi_list[i], vlan=dl_vlanid_list[i])
            for i in range(1, len(dl_tpid_list)):
                pkt[Dot1Q:i].type=dl_tpid_list[i]
            pkt.type=dl_tpid_list[0]

        pkt = pkt / efcp_pkt

        codecs_decode_range = [ x for x in range(pktlen - len(pkt)) ]
        codecs_decode_str = "".join(["%02x"%(x%256) for x in range(pktlen - len(pkt)) ])
        codecs_decode = codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
        pkt = pkt / codecs.decode("".join(["%02x"%(x%256) for x in range(pktlen - len(pkt))]), "hex")
        return pkt

    # Adapted from simple_tcp_packet in $SDE_INSTALL/lib/python3.6/site-packages/ptf/testutils.py
    def simple_efcp_packet(self,
                      pktlen=100,
                      eth_dst="00:01:02:03:04:05",
                      eth_src="00:06:07:08:09:0a",
                      dl_vlan_enable=False,
                      vlan_vid=0,
                      vlan_pcp=0,
                      dl_vlan_cfi=0,
                      ipc_src_addr=1,
                      ipc_dst_addr=2
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

    def setUp(self):
        client_id = 0
        p4_name = "tna_efcp_test"
        BfRuntimeTest.setUp(self, client_id, p4_name)
    #FIXME: Update with correct parameters
    def delete_rules(self, key_tuple_list, efcp_exact_table, target, gc):
        # Delete table entries
        for item in key_tuple_list:
            efcp_exact_table.entry_del(
                target, [
                    efcp_exact_table.make_key([
                        gc.KeyTuple("hdr.efcp.dst_addr", item.dst_addr)
                    ])
                ]
            )

    def runTest(self):
        ig_port = swports[1]
        seed = random.randint(1, 65535)
        logger.info("Seed used %d", seed)
        random.seed(seed)
        num_entries = random.randint(1, 30)
        #num_entries = 12

        # Get bfrt_info and set it as part of the test
        bfrt_info = self.interface.bfrt_info_get("tna_efcp_test")
        efcp_exact_table = bfrt_info.table_get("SwitchIngress.efcp_exact")
        efcp_exact_table.info.key_field_annotation_add("hdr.efcp.dst_addr", "bit")
        efcp_exact_table.info.data_field_annotation_add("vlan_id", "SwitchIngress.efcp_forward", "bit")
        efcp_exact_table.info.data_field_annotation_add("dst_mac", "SwitchIngress.efcp_forward", "mac")
        efcp_exact_table.info.data_field_annotation_add("dst_port", "SwitchIngress.efcp_forward", "bit")

        key_random_tuple = namedtuple("key_random", "dst_addr")
        data_random_tuple = namedtuple("data_random", "vlan_id dst_mac dst_port")
        key_tuple_list = []
        data_tuple_list = []
        unique_keys = {}
        exact_dict= {}

        efcp_id_list = list(set([random.randint(0, 255) for x in range(num_entries)]))
        vlan_id_list = [random.randint(0, 4095) for x in range(len(efcp_id_list))]
        logger.info("Installing %d Exact entries" % (len(efcp_id_list)))

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

        # check get
        resp  = efcp_exact_table.entry_get(target)
        for data, key in resp:
            exact_dict_keys = [ _ for _ in exact_dict.keys() ]
            assert exact_dict[key] == data
            exact_dict.pop(key)
        assert len(exact_dict) == 0

        test_tuple_list = list(zip(key_tuple_list, data_tuple_list))
        
        logger.info("Sending packets for the installed entries to verify")
        # send pkt and verify sent
        for key_item, data_item in test_tuple_list:
            efcp_src_id = random.randint(0, 255)
            src_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % tuple([random.randint(0, 255) for x in range(6)])

            # NOTE: do not change this (a priori)
            # A1
            #pkt = self._get_tx_packet(key_item.dst_addr, None, None, None, None)
            # A2
            #pkt = self._get_tx_packet(key_item.dst_addr, efcp_src_id, data_item.dst_mac, src_mac, vlan_id)
            # B
            pkt = self.simple_efcp_packet(eth_src=src_mac, eth_dst=data_item.dst_mac, \
                    vlan_vid=vlan_id, \
                    ipc_src_addr=efcp_src_id, ipc_dst_addr=key_item.dst_addr \
                    )
            #exp_pkt = testutils.simple_tcp_packet(eth_dst=data_item.dmac,
            #                                      eth_src=data_item.smac,
            #                                      ip_dst=key_item.dst_ip)
            print("ScaPy emitted packet")
            pkt.show()
            
            # NOTE: do not change this (a priori)
            # A1
            #exp_pkt = self._get_tx_packet(None, efcp_src_id, data_item.dst_mac, src_mac, vlan_id)
            # A2
            #exp_pkt = self._get_tx_packet(key_item.dst_addr, efcp_src_id, data_item.dst_mac, src_mac, vlan_id)
            # B
            # NOTE: "eth_src" will be equal to "eth_dst" because of the logic in tna_efcp.p4
            exp_pkt = self.simple_efcp_packet(eth_dst=data_item.dst_mac, eth_src=data_item.dst_mac, \
                    vlan_vid=vlan_id, \
                    ipc_src_addr=efcp_src_id, ipc_dst_addr=key_item.dst_addr \
                    )
            #exp_pkt = testutils.simple_tcp_packet(eth_dst=data_item.dst_mac)
            print("ScaPy expected packet")
            exp_pkt.show()
            
            logger.info("Sending packet on port %d", ig_port)
            testutils.send_packet(self, ig_port, pkt)

            logger.info("Verifying entry for IPC ID %s" % key_item.dst_addr)
            logger.info("Expecting packet on port %d", data_item.dst_port)
            try:
                testutils.verify_packets(self, exp_pkt, [data_item.dst_port])
            except Exception as e:
                logger.error("Error on packet verification")
                raise e
            #finally:
                #self.delete_rules(key_tuple_list, efcp_exact_table, target, gc)

        logger.info("All expected packets received")
        logger.info("Deleting %d Exact entries" % (len(efcp_id_list)))
        # Delete table entries
        for i in range(0, len(efcp_id_list)):
            efcp_exact_table.entry_del(
                target,
                [efcp_exact_table.make_key([gc.KeyTuple("hdr.efcp.dst_addr", efcp_id_list[i])])])
