#!/bin/bash

sleep 2
source $VENV/bin/activate
echo "Initialising configuration transmission to bmv2"
python3 runtime-shell.py
