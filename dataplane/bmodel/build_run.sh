#!/bin/bash

#sudo bash
cd ~/ncsc-1.3.3rc4
source ./bin/setup.sh
export NPL_EXAMPLES=/home/npl/rina_router/dataplane/bmodel
cd $NPL_EXAMPLES/efcp_router
make fe_nplsim
make nplsim_comp
make nplsim_run
