# Ports configuration.
#
# Those numbers aren't random. They were picked to mimic what we have
# on the Edgecore switch as 2 first ports.
#
# -----+----+---+----+-------+----+--+--+---+---+---+--------+----------------+----------------+-
# PORT |MAC |D_P|P/PT|SPEED  |FEC |AN|KR|RDY|ADM|OPR|LPBK    |FRAMES RX       |FRAMES TX       |E-----+-# ---+---+----+-------+----+--+--+---+---+---+--------+----------------+----------------+-
# 1/0  |23/0|132|3/ 4|100G   |NONE|Au|Au|YES|ENB|UP |  NONE  |             276|               4|
# 2/0  |22/0|140|3/12|100G   |NONE|Au|Au|NO |ENB|DWN|  NONE  |               0|               0|
# 3/0  |21/0|148|3/20|100G   |NONE|Au|Au|YES|ENB|UP |  NONE  |               5|               0|
# ...
PORT_A_VETH="veth0"
PORT_A_NO=132

PORT_B_VETH="veth2"
PORT_B_NO=148

# General configuration
EDF9_FAKE_MAC="00:00:00:00:00:09"
EDF9_REAL_MAC="6c:fe:54:0a:20:70"
EDF9_IP="192.168.2.209"
EDF9_RANGE="192.168.2.0"
EDF9_RINA_ADDR=13
EDF9_VLAN_IP="192.168.4.209"

EDF10_FAKE_MAC="00:00:00:00:00:10"
EDF10_REAL_MAC="6c:fe:54:0a:40:90"
EDF10_IP="192.168.2.210"
EDF10_RANGE="192.168.3.0"
EDF10_RINA_ADDR=14
EDF10_VLAN_IP="192.168.5.210"

# This is a random MAC for the switch. It is to be used in situations
# where the Edgecore needs to act like it if was a router.
SWITCH_MAC="01:02:03:04:05:06"

# For L3-IP test, a random ip in the 192.168.2.xxx range.
EDF_DOT2X_IP_1="192.168.2.1"
EDF_DOT2X_IP_2="192.168.2.2"
EDF_DOT3X_IP_1="192.168.3.1"
EDF_DOT3X_IP_2="192.168.3.2"

# VLAN
VLAN_ID = 0xFF
