#
P4_NAME=edf-switch
P4_INFO=./out/$(P4_NAME).tofino/p4info.txt
P4_FILE=p4/$(P4_NAME).p4
P4_INCLUDE=$(SDE_INSTALL)/share/p4c/p4include/
P4C_ARGS=-I common/p4 \
		 -I $(P4_INCLUDE) \
         --p4runtime-force-std-externs\
		 --p4runtime-file $(P4_INFO) \
		 --p4runtime-format text \
		 --std p4-16 \
		 --target tofino \
		 --arch tna

# Connection parameters for tofino-model
MODEL_DEVICE_ID=0
MODEL_ADDR=localhost
MODEL_PORT=9339
MODEL_DEVICE_ID=0

# Connection parameters for the EdgeCore switch. Connect with SSH
# forwarding to use the switch remotely with P4Runtime.
BF_DEVICE_ID=1
BF_ADDR=localhost
BF_PORT=9339
BF_DEVICE_ID=1

TOFINO_DIR=./out/$(P4_NAME).tofino
TOFINO_BIN=$(TOFINO_DIR)/pipe/tofino.bin
TOFINO_P4R_BIN=$(TOFINO_DIR)/pipe/tofino-packed.bin

ifeq ($(SDE),)
$(error SDE environment variable not set.)
endif
ifeq ($(SDE_INSTALL),)
$(error SDE_INSTALL environment variable not set.)
endif

.PHONY: clean shell

all: $(TOFINO_BIN) $(TOFINO_P4R_BIN)

clean:
	rm -r ./out

# BUILD INSTRUCTIONS

./out:
	mkdir -p $(TOFINO_DIR)

$(TOFINO_BIN): out $(P4_FILE)
# Note that it's important the output file parameter has a full path
# and not a relative one because the output .conf file will have the
# same relative path and that will confuse tofino-model. This is the
# reason why I'm using $PWD here.
	$(SDE_INSTALL)/bin/bf-p4c $(P4C_ARGS) $(PWD)/$(P4_FILE) -o $(PWD)/out/$(P4_NAME).tofino \

$(TOFINO_P4R_BIN): $(TOFINO_BIN)
	p4r-tofino-pack --ctx-json $(TOFINO_DIR)/pipe/context.json \
				    --tofino-bin $(TOFINO_BIN) \
					--out $(TOFINO_P4R_BIN) \
				    --name $(P4_NAME)

# MAKEFILE COMMAND LINE TOOLS

# Start p4runtime-shell on the tofino-model
model-shell: $(TOFINO_P4R_BIN)
	python3 -m p4runtime_sh --grpc-addr $(MODEL_ADDR):$(MODEL_PORT) \
                --device-id $(MODEL_DEVICE_ID) --election-id 0,1 --config $(P4_INFO),$(TOFINO_P4R_BIN)

# Start p4runtime-shell on the Edgecore switch
bf-shell: $(TOFINO_B4R_BIN)
	python3 -m p4runtime_sh --grpc-addr $(BF_ADDR):$(BF_PORT) \
                --device-id $(BF_DEVICE_ID) --election-id 0,1 --config $(P4_INFO),$(TOFINO_P4R_BIN)

# P4Runtime program to start on the tofino-model
model-controlplane_%: $(TOFINO_P4R_BIN)
	cp_static/$(subst model-,,$@).py $(MODEL_ADDR) $(MODEL_PORT) $(MODEL_DEVICE_ID)

# P4Runtime program to start on the Edgecore switch
bf-controlplane_%: $(TOFINO_P4R_BIN)
	cp_static/$(subst bf-,,$@).py $(BF_ADDR) $(BF_PORT) $(BF_DEVICE_ID)

# SDE tools

.PHONY: run_switchd run_tofino_model

# That's weird but bfswitchd won't ever find the context JSON file in
# the right place unless we do that.
$(SDE_INSTALL)/$(TOFINO_DIR):
	ln -s $(PWD)/$(TOFINO_DIR)

run_tofino_model:
	$(SDE)/run_tofino_model.sh --log-dir $(PWD)/logs -c $(TOFINO_DIR)/$(P4_NAME).conf -p $(P4_NAME) -f $(PWD)/config/ports.json

run_switchd:
	$(SDE)/run_switchd.sh -p $(P4_NAME) -c $(TOFINO_DIR)/$(P4_NAME).conf -- --p4rt-server=$(BF_ADDR):$(BF_PORT) --skip-p4

