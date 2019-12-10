# NPL simple implementation for a RINA interior router

Simple NPL implementation that only forward EFCP packets (without VLAN)

## Building and running project

Is necessary to first **build the environment**

```bash
sudo bash
  # Password is “npl”
cd ~/ncsc-1.3.3rc4
source ./bin/setup.sh
```

Once the environment is ready, follow the below steps to **build and run** the NPL project:

```bash
export NPL_EXAMPLES=/home/npl/nplRINA
cd $NPL_EXAMPLES/efcp_switch
make fe_nplsim
make nplsim_comp
make nplsim_run
```

Now you will see two xterm windows one with name `BMODEL` and another with `BMCLI`.

Before you inject packets you need populate Logical tables using pre-defined table configurations **through `BMCLI` window**.  
Command to use is as below:

```
rcload /home/npl/nplRINA/efcp_switch/bm_tests/tests/tbl_cfg.txt
```

Now is possible to inject packet using below command from **original console window** where you compiled the NPL code. If you run the test in another terminal instance, make sure you have the enviroment ready.

```bash
python bm_tests/tests/efcp_test.py
```

The above `efcp_test.py` sends an ingress packet to port 1 through the `loopback` interface. And you can see packet being switched to Port 2 based on NPL switching program in `BMODEL` xterm window.  


## Useful stuff

### Print
NPL uses print construct to print value of any variable in the program. print command is translated to the
Behavioral C Model. It does not have significance from compilation point of view. print invocation is called in
the C model in the same sequence as in an NPL program. Only fields can be printed, no structs. Internally,
print command works in a similar way as the C printf command.

Construct is `print - print in behavioral model`.

**Example:**
```c
print("Value of the SVP is %d, VFI is %d\n", obj_bus.svp, obj_bus.vfi);
```

### Create Checksum
The Create Checksum construct can only be used in function constructs. The create checksum supports TCP/UDP checksum calculations.

**Example:**
```
create_checksum(egress_pkt.group2.ipv4.hdr_checksum,
  {egress_pkt.group2.ipv4.version, egress_pkt.group2.ipv4.hdr_len,
  egress_pkt.group2.ipv4.tos, egress_pkt.group2.ipv4.v4_length,
  egress_pkt.group2.ipv4.id, egress_pkt.group2.ipv4.flags,
  egress_pkt.group2.ipv4.frag_offset, egress_pkt.group2.ipv4.ttl,
  egress_pkt.group2.ipv4.protocol, egress_pkt.group2.ipv4.sa,
  egress_pkt.group2.ipv4.da});
```

```
create_checksum(egress_pkt.fwd_l3_l4_hdr.udp.checksum,
  {egress_pkt.fwd_l3_l4_hdr.ipv4.sa,
  egress_pkt.fwd_l3_l4_hdr.ipv4.da,
  editor_dummy_bus.zero_byte,
  egress_pkt.fwd_l3_l4_hdr.ipv4.protocol,
  egress_pkt.fwd_l3_l4_hdr.udp.udp_length,
  egress_pkt.fwd_l3_l4_hdr.udp.src_port,
  egress_pkt.fwd_l3_l4_hdr.udp.dst_port,
  egress_pkt.fwd_l3_l4_hdr.udp.udp_length,
  egress_pkt.fwd_l3_l4_hdr.udp._PAYLOAD});
```

## Outputs

### `bmodel.log` success

```
sc_main() enter
Looking up sfc_iarb_profile
Looking up sfc_mmu_profile
Looking up sfc_edb_profile
ngsdk_p2l_set sid: 2 buf_len: 5, typ 0, index: 128
0x00090100 0x00000000 
ngsdk_p2l_set sid: 2 buf_len: 5, typ 0, index: 128
0x00090200 0x00000000 
ngsdk_p2l_set sid: 1 buf_len: 14, typ 0, index: 16384
0x00010000 0x05000001 0x04810000 0x00000000 
ngsdk_p2l_set sid: 1 buf_len: 14, typ 0, index: 16384
0x00020000 0x05000002 0x04820000 0x00000000 
pkt_has_crc: crc: 0
crc_gen_en: crc_en: 0
*************** Transmit Packet to Port ***************
Sending packet into BM (Src Port = 1)
Ingress Packet, port(1), len(71)
0000:  00 00 05 00 00 02 00 00  05 00 00 01 0d 1f 01 00     ................
0010:  02 00 01 00 00 00 00 00  80 00 00 01 00 00 00 00     ................
0020:  00 00 54 68 69 73 20 45  46 43 50 20 70 61 63 6b     ..This EFCP pack
0030:  65 74 20 23 32 20 66 72  6f 6d 20 43 4c 49 20 74     et #2 from CLI t
0040:  6f 20 42 4d 20 3a 29                                 o BM :)
bm_debug_lvl = 4
###### Starting Ingress Pipeline Processing ######
Looking up sfc_iarb_profile
Looking up port_table
Looking up efcp_forward_table
Looking up sfc_mmu_profile
##### Starting Egress Pipeline Processing #####
Looking up sfc_mmu_profile
Looking up sfc_edb_profile
Egress Packet, port(2), len(71):

0000:  00 00 05 00 00 02 00 00  05 00 00 01 0d 1f 01 00     ................
0010:  02 00 01 00 00 00 00 00  80 00 00 01 00 00 00 00     ................
0020:  00 00 54 68 69 73 20 45  46 43 50 20 70 61 63 6b     ..This EFCP pack
0030:  65 74 20 23 32 20 66 72  6f 6d 20 43 4c 49 20 74     et #2 from CLI t
0040:  6f 20 42 4d 20 3a 29                                 o BM :)
Receiving packet from BM
Dst Port = 2
Dst Modid = 9
```


