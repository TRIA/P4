# Accepted PDU Types

DATA_TRANSFER = 0x80
LAYER_MANAGEMENT = 0x40
ACK_ONLY = 0xC1
NACK_ONLY = 0xC2
ACK_AND_FLOW_CONTROL = 0xC5
NACK_AND_FLOW_CONTROL = 0xC6
FLOW_CONTROL_ONLY = 0xC4
SELECTIVE_ACK = 0xC9
SELECTIVE_NACK = 0xCA
SELECTIVE_ACK_AND_FLOW_CONTROL = 0xCD
SELECTIVE_NACK_AND_FLOW_CONTROL = 0xCE
CONTROL_ACK = 0xC0
RENDEVOUS = 0xCF


local_vars = locals()
local_vars = [ _ for _ in filter(lambda x: not x.startswith("__") and x != "local_vars", local_vars) ]
# Generate the EDF_PDU_TYPES dictionary, similar to Ethernet's ETHER_TYPES
EFCP_TYPES = {}
for local_var in local_vars:
    local_val = locals()[local_var]
    EFCP_TYPES[local_var] = local_val
