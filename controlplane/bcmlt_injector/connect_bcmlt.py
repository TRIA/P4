#!/usr/bin/env python3

from subprocess import PIPE
from subprocess import Popen
import os
import sys

lookup = "lt VLAN lookup VLAN_ID=7"

def read_stdout_lines(shell, times=1):
    lines = []
    for i in xrange(times):
        lines += [str(shell.stdout.readline()).strip()]
    return "\n".join([line for line in lines])

def inject_sdklt_file(sdklt_file):
    ignore_lines = 0

    print("\n\n+ Accessing BCMLT shell")

    # Allow to send data to the pipe and receive data from it
    bcmlt_shell = Popen(["/bin/bash", "/home/npl/sdklt-testing/build_run_sdklt.sh"], stdin=PIPE, stdout=PIPE)

    print("\n+ Loading model")
    while True:
        line = read_stdout_lines(bcmlt_shell)
        if "Found" in line or "device" in line:
            break
        print(line)

    print("\n+ Accessing \"cint\"")
    bcmlt_shell.stdin.write(str.encode("cint") + b"\n")
    #bcmlt_shell.stdin.flush() # not necessary in this case
    while True:
        line = read_stdout_lines(bcmlt_shell)
        if len(line) > 0 and "ERROR" not in line:
            print(line)
            bcmlt_shell.stdin.write(b"1\n")
            ignore_lines+=1
        else:
            break

    c_lines = []
    with sdklt_file as f:
        c_lines = [line.rstrip() for line in f]

    print("\n+ Loading C bindings from file %s" % str(sdklt_file))
    bcmlt_shell.stdin.write(b";\n")
    for c_line in c_lines:
        #if c_line.strip().startswith("//"):
        #    print("[ignored] " + c_line)
        #    continue
        print("> " + c_line)
        bcmlt_shell.stdin.write(str.encode(c_line) + b"\n")
#        while True:
#            line = read_stdout_lines(bcmlt_shell)
#            if len(line) > 0 and "ERROR" not in line:
#                print(line)
#                bcmlt_shell.stdin.write(b"1\n")
#                ignore_lines+=1
#            else:
#                break
    
    print("\n+ Exiting \"cint\"")
    bcmlt_shell.stdin.write(str.encode("exit;") + b"\n")
    line = read_stdout_lines(bcmlt_shell)
    if len(line) > 0 and "ERROR" not in line:
        print(line)

    # Do lookup to check if entries have been updated correctly
    print("BCMLT command: " + lookup)
    bcmlt_shell.stdin.write(str.encode(lookup) + b"\n")
    for i in range(0, 18):
        line = read_stdout_lines(bcmlt_shell)
        if len(line) > 0:
            print(line)
        else:
            break

    print("\n+ Exiting BCMLT shell")
    bcmlt_shell.stdin.write(str.encode("exit") + b"\n")
    line = read_stdout_lines(bcmlt_shell)
    if len(line) > 0 and "ERROR" not in line:
        print(line)

def file_error():
    print("Error: provide the path to an existing file with C bindings for SDKLT")
    sys.exit(1)

if __name__ == "__main__":
   if len(sys.argv) < 2:
       file_error()
   sdklt_file_path = sys.argv[1]
   if not os.path.isfile(sdklt_file_path):
       file_error()
   sdklt_file = open(sdklt_file_path)
   inject_sdklt_file(sdklt_file)