#!/bin/bash

function killpid() {
  PID=("$@")
  for pid_ in ${PID[@]}; do
    [[ ! -z ${pid_} ]] && echo "Terminating PID ${pid_} for Tofino switch" && sudo kill -9 ${pid_}
  done
}

PID=$(sudo ps aux | grep run_switchd | grep -v grep | grep -v $0 | grep -v tmux | awk {'print $2'})
killpid ${PID}
PID=$(sudo ps aux | grep bf_switchd | grep -v grep | grep -v $0 | grep -v tmux | awk {'print $2'})
killpid ${PID}
