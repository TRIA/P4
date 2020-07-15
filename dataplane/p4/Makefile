P4C_IMG := opennetworking/p4c:stable
DOCKER_NET := rina-controlplane
MN_STR_NAME := mn-stratum
P4R_STR_NAME := p4runtime-sh

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
curr_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
curr_dir_sha := $(shell echo -n "$(curr_dir)" | shasum | cut -c1-7)

default:
	$(error Please specify a make target (see README.md))

docker_net_create:
	@echo "setup"
	@docker network create ${DOCKER_NET} || true

docker_net_rm:
	@docker network rm ${DOCKER_NET} || true

env-start: docker_net_create efcp-build
	cd env && docker-compose up --build -d && cd ..
#docker attach --detach-keys "ctrl-z" ${MN_STR_NAME} || echo "*** Detached from Mininet CLI" 

#	echo "MN_STRATUM_TMP_DIR=$(shell mktemp -d mn-stratum.XXX)" > env/.env 

env-stop:
	cd env && docker-compose exec p4runtime mn-stratum rm /tmp/* || true && cd ..
	cd env && docker-compose down -t0 && docker-compose rm -fsv && cd ..
#	cd env && docker-compose down -t0 && rm -f .env && rm -rf mininet/tmp/* && cd ..
	@make docker_net_rm

env-reset: env-stop
	-rm -rf ./mininet/tmp

env-restart: env-reset env-start

mn-cli:
	$(info *** Attaching to Mininet CLI...)
	$(info *** To detach press Ctrl-Z (Mininet will keep running))
	-@docker attach --detach-keys "ctrl-z" ${MN_STR_NAME} || echo "*** Detached from Mininet CLI"

mn-attach:
	-@docker exec -it ${MN_STR_NAME} bash

mn-log:
	docker logs -f ${MN_STR_NAME}

mn-bmv2-log:
	if [ -f ${PWD}/env/mininet/tmp/s1/stratum_bmv2.log ]; then tail -1000f ${PWD}/env/mininet/tmp/s1/stratum_bmv2.log; fi

mn-bmv2-sniff-eth1:
	$(info "Capturing one packet in ${MN_STR_NAME}:s1-eth1 and showing contents afterwards")
	-@docker exec -it ${MN_STR_NAME} tshark -i s1-eth1 -c 1 -w output_eth1

mn-bmv2-sniff-eth2:
	$(info "Capturing one packet in ${MN_STR_NAME}:s1-eth2 and showing contents afterwards")
	-@docker exec -it ${MN_STR_NAME} tshark -i s1-eth2 -c 1 -w output_eth2
	-@docker exec -it ${MN_STR_NAME} cat output_eth2 || true

p4runtime-log:
	docker logs -f ${P4R_STR_NAME}

clean:
	-rm -rf model/build

efcp-build: model/efcp.p4
	$(info *** Building P4 program...)
	@mkdir -p model/build
	docker run --rm -v ${curr_dir}:/workdir -w /workdir ${P4C_IMG} \
		p4c-bm2-ss --arch v1model -o model/build/bmv2.json \
		--p4runtime-files model/build/p4info.txt --Wdisable=unsupported \
		model/efcp.p4
	@echo "*** P4 program compiled successfully! Output files are in model/build"

mn-start: docker_net_create
	docker run --privileged --rm -it -v ${PWD}/env/mininet:/mininet -v ../common/tests:/root/tests -v ${PWD}/env/mininet/tmp/${MN_STR_NAME}:/tmp -v ../common/tests/pdu_types.py:/root/pdu_types.py -v ../common/tests/send_efcp_h1.py:/root/send_efcp_h1.py -v ../common/tests/send_efcp_h2.py:/root/send_efcp_h2.py -p50001:50001 --name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} sergiogimenez/mininet-mod:latest
#	current=${PWD} && \
#		cd env/mininet && \
#		docker build -t edf-dp-mn-stratum:latest . && \
#		docker run --privileged --rm -it -v ${PWD}/env/mininet/scripts:/root/scripts -v /tmp/${MN_STR_NAME}:/tmp -p50001-50030:50001-50030 --name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} edf-dp-mn-stratum:latest && \
#		cd ${current}
#	-xhost +si:localuser:root
#	# OLD
#	docker run --privileged --rm -it -v ${PWD}/env/mininet/scripts:/root/scripts -v /tmp/${MN_STR_NAME}:/tmp -p50001-50030:50001-50030 --name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} opennetworking/${MN_STR_NAME}
#	-xhost +si:localuser:root

mn-stop:
	docker rm -f ${MN_STR_NAME} || true
	@make docker_net_rm

edf-dp-test-stratum: docker_net_create efcp-build
	cd env && docker-compose up --build -d && cd ..
