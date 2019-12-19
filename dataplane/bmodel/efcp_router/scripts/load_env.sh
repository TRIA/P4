#!/bin/bash

base_path=$(realpath $(realpath $(dirname "$0")/..))

#sudo bash
cd /home/npl/ncsc-1.3.3rc4
source ./bin/setup.sh
export NPL_EXAMPLES=$base_path
cd $NPL_EXAMPLES
