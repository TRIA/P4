# P4
RINA P4 router code

This code is the final result from a project jointly funded by Ciena, Inc., and Innovation ENCQOR of Quebec, Canada, one goal of which was to research using RINA in a data center as the
lowest-level (over Ethernet) protocol for inter-machine communication, replacing or augmenting IP.  It is derived from an Apache-licensed prototype done
by i2CAT in Barcelona, Spain, available on github (permission required) at https://github.com/orgs/PouzinSociety/repositories

The initial code has been ported to run on the Barefoot/Intel chipset, implemented in an Edgecore router (clone of the Barefoot reference platform).
Additional code is provided to exercise the control plane, but header files and other information that is Intel proprietary will be needed
to successfully build and run tha code as-is.  To obtain the SDK, you will need to sign an NDA with Intel and download the specs and SDK.

Further detail will be added to this README to reference papers and provide further information.
