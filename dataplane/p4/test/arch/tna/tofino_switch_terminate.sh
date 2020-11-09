#!/bin/bash

PID=$(ps -ef | grep run_switchd | grep -v grep | grep -v $0 | awk {'print $2'})
for pid_ in ${PID}; do
    [[ ! -z ${pid_} ]] && echo "Terminating PID ${pid_} for Tofino switch" && sudo kill -9 ${pid_}
done
