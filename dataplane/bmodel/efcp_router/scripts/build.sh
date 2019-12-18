#!/bin/bash

scripts_path=$(realpath $(realpath $(dirname "$0")))

source ${scripts_path}/load_env.sh

make fe_nplsim
make nplsim_comp
