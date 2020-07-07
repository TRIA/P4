#!/bin/bash

# Relative paths
# - Get one folder up for the base path of the "efcp_router"
base_path=$(realpath $(realpath $(dirname "$0"))/..)
test_path=${base_path}/bm_tests/tests

# CONFIG
# - Run a specific test class
class=""
# - Enable or disable graphic environment for tests
test_with_xterm=false


function parse_args {
    POSITIONAL=()
    while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -g|--graphical)
            test_with_xterm="true"
            shift # past argument
            ;;
        -c|--class)
            class="$2"
            shift # past argument
            shift # past value
            ;;
        --default)
            test_with_xterm=false
            ;;
        *)    # unknown option
            POSITIONAL+=("$1") # save it in an array for later
            shift # past argument
            ;;
    esac
    done
    set -- "${POSITIONAL[@]}" # restore positional parameters
}

function load_model {
    cmd="${base_path}/fe_output/bmodel/bin/bmodel.sim -p 9090 \
                    +log_file=bmodel.log"
    if [ "${test_with_xterm}" = true ]; then
        xterm -title "BMODEL (port 9090)" -e $cmd &
    else
        $cmd &
    fi
}

function load_cli {
    cmd="python ${test_path}/cli/bmif_cli.py --port 9090 \
                --regfile ${base_path}/fe_output/bmodel/dfiles/bm_all.yml"

    if [ "${test_with_xterm}" = true ]; then
        xterm -title "BMCLI (port 9090)" -e $cmd &
    else
        # Ignoring output from the error console
        $cmd > /dev/null 2>&1 &
    fi
}

# Not invoked by default because the NPLBaseTest class
# already provides a "CFG_FILE" in its initialisation
function load_cli_with_cfg {
    cmd="python ${test_path}/cli/bmif_cli.py --port 9090 \
                --regfile ${base_path}/fe_output/bmodel/dfiles/bm_all.yml \
                --rcfile ${base_path}/bm_tests/tests/tbl_cfg.txt"

    if [ "${test_with_xterm}" = true ]; then
        xterm -title "BMCLI (port 9090)" -e $cmd &
    else
        # Ignoring output from the error console
        $cmd > /dev/null 2>&1 &
    fi
}

function clear_sim {
    if [ "${test_with_xterm}" = true ]; then
        killall xterm
    fi
    # Clean up any possible zombie process on the background
    pkill -f bmodel
    pkill -f bmcli
}

function load_sim {
    cd $base_path
    # FIXME: calling bmcli must return a sort of error when configuration is not properly loaded!!
    load_model
    load_cli
}

function clear_env {
    echo "Cleaning after use..."
    sleep 1
    clear_sim
}

# Allows obtaining a test using any of the following:
# - /home/npl/rina_router/dataplane/bmodel/efcp_router/bm_tests/tests/efcp_test.py
# - efcp_test.py
# - efcp_test
# - efcp
function get_class_abspath {
    loaded_class=$1
    if [ ! -z ${loaded_class} ]; then
        [[ -f ${test_path}/${loaded_class} ]] && loaded_class="${test_path}/${loaded_class}"
        [[ -f ${test_path}/${loaded_class}.py ]] && loaded_class="${test_path}/${loaded_class}.py"
        [[ -f ${test_path}/${loaded_class}_test.py ]] && loaded_class="${test_path}/${loaded_class}_test.py"
    else
        echo "Error: test does not exist"
        exit 1
    fi
    echo ${loaded_class}
}

function load_test {
    loaded_class=$1
    loaded_class=$(get_class_abspath ${loaded_class})
    [[ ! -f ${loaded_class} ]] && echo "Error: test does not exist" && exit 1
    # Load everything from the simulator prior to each test
    clear_sim
    load_sim
    sleep 1
    # Note: "${model}" is not really needed / redundant in this particular setup
    echo "Running test ${loaded_class}..."
    python ${loaded_class} ${model}
    sleep 1
}

function load_tests {
    for class in ${test_path}/*_test.py; do
       load_test $class
    done
}

# SETUP
# - Clear environment after use
trap clear_env EXIT

# MAIN PROGRAM

# - Parse arguments for options
parse_args $@

# - Load a specific test, if given; or all of them
if [[ ! -z ${class} ]]; then
    load_test ${class}
else
    load_tests
fi
