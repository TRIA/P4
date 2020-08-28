#!/bin/bash

current=$PWD

# autoreconf
apt-get update -y
apt-get install -y libtool
apt-get install -y autoconf

# LibYAML 0.1.7 C library
cd $HOME
git clone https://github.com/yaml/libyaml.git
cd $HOME/libyaml
./bootstrap
./configure
make
make install

# PyYAML Python library
apt-get update -y
apt install -y python-pip
pip install pyyaml

cd $current
