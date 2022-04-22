P4_NAME=rina

# v1model
V1MODEL_DIR=./out/$(P4_NAME).v1model
V1MODEL_OUT=$(V1MODEL_DIR)/$(P4_NAME)-v1model.json
V1MODEL_P4INFO_OUT=./out/$(P4_NAME).v1model/p4info.txt

V1MODEL_P4_FILE=p4/$(P4_NAME)-v1model.p4
V1MODEL_P4C_ARGS=-D__TARGET_V1MODEL__ \
			     --arch v1model \
				 --std p4-16 \
				 --output $(shell dirname $(V1MODEL_OUT)) \
				 --p4runtime-files $(V1MODEL_P4INFO_OUT)

# Barefoot
BF_DIR=./out/$(P4_NAME).tofino
BF_UNPACKED_OUT=$(BF_DIR)/pipe/tofino.bin
BF_OUT=$(BF_DIR)/pipe/tofino-packed.bin
BF_P4INFO_OUT=./out/$(P4_NAME).tofino/p4info.txt

BF_P4_FILE=p4/$(P4_NAME)-bf.p4
BF_P4_INCLUDE=$(SDE_INSTALL)/share/p4c/p4include/
BF_P4C_ARGS=-D__TARGET_TOFINO__ \
		    -I common/p4 \
	        -I $(BF_P4_INCLUDE) \
            --p4runtime-force-std-externs\
            --p4runtime-file $(BF_P4INFO_OUT) \
		 	--p4runtime-format text \
		 	--std p4-16 \
		 	--target tofino \
		 	--arch tna

# Connection parameters for tofino-model
BFMODEL_DEVICE_ID=0
BFMODEL_ADDR=localhost
BFMODEL_PORT=9339
BFMODEL_DEVICE_ID=0

# Connection parameters for the EdgeCore switch. Connect with SSH
# forwarding to use the switch remotely with P4Runtime.
BF_DEVICE_ID=1
BF_ADDR=localhost
BF_PORT=9339
BF_DEVICE_ID=1

V1MODEL_DEVICE_ID=1
V1MODEL_ADDR=localhost
V1MODEL_PORT=9559
V1MODEL_DEVICE_ID=0

ifeq ($(shell which bf-p4c),)
  $(warning "Barefoot compiler (bf-p4c) is not in the PATH, won't build TNA files")
else
  ifeq ($(SDE_INSTALL),)
    $(error "SDE_INSTALL environment variable is NOT set, can't build TNA files")
  endif
  ifeq ($(SDE),)
    $(error "SDE environment variable is NOT set, can't build TNA files")
  endif
  BF_TARGETS:=$(BF_OUT) $(BF_P4INFO_OUT)
endif

ifeq ($(shell which p4c),)
  $(warning "P4 compiler (p4c) is not in the PATH, won't build V1Model files")
else
  V1MODEL_TARGETS=$(V1MODEL_OUT) $(V1MODEL_P4INFO_OUT)
endif

$(info BF_TARGETS: $(BF_TARGETS))
$(info V1MODEL_TARGETS: $(V1MODEL_TARGETS))

.PHONY: clean shell

all: $(V1MODEL_TARGETS) $(BF_TARGETS)

clean:
	rm -r ./out

# BUILD INSTRUCTIONS

./out:
	mkdir -p $(BF_DIR)
	mkdir -p $(V1MODEL_DIR)

$(V1MODEL_OUT): out $(V1MODEL_P4_FILE)
	p4c $(V1MODEL_P4C_ARGS) $(V1MODEL_P4_FILE)

$(BF_UNPACKED_OUT): out $(BF_P4_FILE)
# Note that it's important the output file parameter has a full path
# and not a relative one because the output .conf file will have the
# same relative path and that will confuse tofino-model. This is the
# reason why I'm using $PWD here.
	$(SDE_INSTALL)/bin/bf-p4c $(BF_P4C_ARGS) $(PWD)/$(BF_P4_FILE) -o $(PWD)/$(BF_DIR) \

$(BF_OUT): $(BF_UNPACKED_OUT)
	p4r-tofino-pack --ctx-json $(BF_DIR)/pipe/context.json \
				    --tofino-bin $(BF_UNPACKED_OUT) \
					--out $(BF_OUT) \
				    --name $(P4_NAME)

# MAKEFILE COMMAND LINE TOOLS

# Start p4runtime-shell on the tofino-model
bfmodel-shell: $(BF_P4R_OUT)
	python3 -m p4runtime_sh --grpc-addr $(BFMODEL_ADDR):$(BFMODEL_PORT) \
                --device-id $(BFMODEL_DEVICE_ID) --election-id 0,1 --config $(BF_P4INFO_OUT),$(BF_OUT)

# Start p4runtime-shell on the Edgecore switch
bf-shell: $(BF_P4R_OUT)
	python3 -m p4runtime_sh --grpc-addr $(BF_ADDR):$(BF_PORT) \
                --device-id $(BF_DEVICE_ID) --election-id 0,1 --config $(BF_P4INFO_OUT),$(BF_OUT)

v1model-shell: $(V1MODEL_P4R_OUT)
	python3 -m p4runtime_sh --grpc-addr $(V1MODEL_ADDR):$(V1MODEL_PORT) \
			    --device-id $(V1MODEL_DEVICE_ID) --election-id 0,1 --config $(V1MODEL_P4INFO_OUT),$(V1MODEL_P4INFO_OUT)

# P4Runtime program to start on the tofino-model
bfmodel-controlplane_%: $(BF_P4R_OUT)
	cp_static/$(subst bfmodel-,,$@).py $(BF_OUT) $(BF_P4INFO_OUT) $(BFMODEL_ADDR) $(BFMODEL_PORT) $(BFMODEL_DEVICE_ID)

# P4Runtime program to start on the Edgecore switch
bf-controlplane_%: $(BF_OUT)
	cp_static/$(subst bf-,,$@).py $(BF_OUT) $(BF_P4INFO_OUT) $(BF_ADDR) $(BF_PORT) $(BF_DEVICE_ID)

v1model-controlplane_%: $(V1MODEL_OUT)
	cp_static/$(subst v1model-,,$@).py $(V1MODEL_OUT) $(V1MODEL_P4INFO_OUT) $(V1MODEL_ADDR) $(V1MODEL_PORT) $(V1MODEL_DEVICE_ID)

# SDE tools

.PHONY: run_switchd run_tofino_model

# That's weird but bfswitchd won't ever find the context JSON file in
# the right place unless we do that.
$(SDE_INSTALL)/$(BF_DIR):
	ln -s $(PWD)/$(BF_DIR)

run_tofino_model:
	$(SDE)/run_tofino_model.sh --log-dir $(PWD)/logs -c $(BF_DIR)/$(P4_NAME).conf -p $(P4_NAME) -f $(PWD)/config/ports.json

run_switchd:
	$(SDE)/run_switchd.sh -p $(P4_NAME) -c $(BF_DIR)/$(P4_NAME).conf -- --p4rt-server=$(BF_ADDR):$(BF_PORT) --skip-p4

run_simple_switch:
	simple_switch_grpc --log-console -L debug $(V1MODEL_OUT)
