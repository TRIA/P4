#!/bin/bash

base_path=$(realpath $(realpath $(dirname "$0")/..))
scripts_path=$(realpath $(realpath $(dirname "$0")))
test_path=${base_path}/bm_tests/tests

source ${scripts_path}/load_env.sh

function load_model {
    cmd="${base_path}/fe_output/bmodel/bin/bmodel.sim -p 9090 \
                    +log_file=bmodel.log"
    xterm -title "BMODEL (port 9090)" -e $cmd &
}

function load_cli {
    cmd="python ${test_path}/cli/bmif_cli.py --port 9090 \
                --regfile ${base_path}/fe_output/bmodel/dfiles/bm_all.yml \
                --rcfile ${base_path}/bm_tests/tests/tbl_cfg.txt"

    xterm -title "BMCLI (port 9090)" -e $cmd &
}

#make nplsim_run
load_model
load_cli
