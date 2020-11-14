P4C_IMG := opennetworking/p4c:stable
DOCKER_NET := rina-controlplane
MN_STR_NAME := mn-stratum
P4R_STR_NAME := p4runtime-sh

EDF_DP_BUILD_ARCH_V1MODEL := build/arch/v1model
EDF_DP_P4_TNA_NAME := tna_efcp
EDF_DP_BUILD_ARCH_TNA := ${SDE}/pkgsrc/p4-examples/p4_16_programs/${EDF_DP_P4_TNA_NAME}
EDF_DP_INSTALL_ARCH_TNA := ${SDE_INSTALL}/share/tofinopd/${EDF_DP_P4_TNA_NAME}
EDF_DP_BUILD_SDE_ARCH_TNA := ${SDE}/build/p4-build/${EDF_DP_P4_TNA_NAME}/tofino/${EDF_DP_P4_TNA_NAME}

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

env-v1model-start: docker_net_create efcp-v1model-build
	cd env && docker-compose up --build -d && cd ..
#docker attach --detach-keys "ctrl-z" ${MN_STR_NAME} || echo "*** Detached from Mininet CLI" 

#	echo "MN_STRATUM_TMP_DIR=$(shell mktemp -d mn-stratum.XXX)" > env/.env 

env-v1model-stop:
	cd env && docker-compose exec -d p4runtime mn-stratum rm /tmp/* || true && cd ..
	cd env && docker-compose down -t0 && docker-compose rm -fsv && cd ..
#	cd env && docker-compose down -t0 && rm -f .env && rm -rf mininet/tmp/* && cd ..
	@make docker_net_rm

env-v1model-reset: env-v1model-stop
	-rm -rf ./mininet/tmp

env-v1model-restart: env-v1model-reset env-v1model-start

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
#	docker exec -it ${MN_STR_NAME} tail -1000f /tmp/s1/stratum_bmv2.log

mn-bmv2-sniff-eth:
	$(info "Capturing one packet in ${MN_STR_NAME}:s1-${iface} and showing contents afterwards")
	-@docker exec -it ${MN_STR_NAME} tshark -i s1-${iface} -c 1 -w output_${iface}
	-@docker exec -it ${MN_STR_NAME} tshark -x -r output_${iface} || true

mn-bmv2-sniff-eth1:
	make mn-bmv2-sniff-eth iface="eth1"

mn-bmv2-sniff-eth2:
	make mn-bmv2-sniff-eth iface="eth2"

p4runtime-log:
	docker logs -f ${P4R_STR_NAME}

clean:
	-rm -rf build

efcp-v1model-build: src/arch/v1model/efcp.p4
	$(info *** Building P4 program \(arch=v1model\)...)
	@mkdir -p ${EDF_DP_BUILD_ARCH_V1MODEL}
	docker run --rm -v ${curr_dir}:/workdir -w /workdir ${P4C_IMG} \
		p4c-bm2-ss --arch v1model -o ${EDF_DP_BUILD_ARCH_V1MODEL}/bmv2.json \
		--p4runtime-files ${EDF_DP_BUILD_ARCH_V1MODEL}/p4info.txt --Wdisable=unsupported \
		src/arch/v1model/efcp.p4
	@echo "*** P4 program compiled successfully! Output files are in ${EDF_DP_BUILD_ARCH_V1MODEL}"

efcp-tna-build: src/arch/tna
	$(info *** Building P4 program \(arch=tna\)...)
	@(if [ ! -f ${SDE}/../p4_build-9.2.0.sh ]; then echo "Missing SDE or p4_build-9.2.0.sh script" && exit 1; fi)
	@(current=${PWD} && cd $< && \
		${SDE}/../p4_build-9.2.0.sh --with-p4c --with-graphs -I${PWD}/common efcp.p4 \
		P4_VERSION=p4_16 P4_ARCHITECTURE=tna P4_NAME=${EDF_DP_P4_TNA_NAME} && \
		mkdir -p ${EDF_DP_BUILD_ARCH_TNA} && mkdir -p ${EDF_DP_INSTALL_ARCH_TNA} && \
		cp -p ${EDF_DP_BUILD_SDE_ARCH_TNA}/${EDF_DP_P4_TNA_NAME}.conf ${SDE_INSTALL}/share/p4/targets/tofino/ && \
		cp -p ${EDF_DP_BUILD_SDE_ARCH_TNA}/bf-rt.json ${EDF_DP_INSTALL_ARCH_TNA}/ && \
		cp -p ${EDF_DP_BUILD_SDE_ARCH_TNA}/pipe/context.json ${EDF_DP_INSTALL_ARCH_TNA}/pipe/ && \
		cp -p ${EDF_DP_BUILD_SDE_ARCH_TNA}/pipe/tofino.bin ${EDF_DP_INSTALL_ARCH_TNA}/pipe/ && \
		cp -Rp ${EDF_DP_BUILD_SDE_ARCH_TNA}/* ${EDF_DP_BUILD_ARCH_TNA}/ && \
		cd ${current} && \
	 	echo "*** P4 program compiled successfully! Output files are in ${EDF_DP_BUILD_ARCH_TNA} and ${EDF_DP_INSTALL_ARCH_TNA}")

mn-start: docker_net_create
	docker run --privileged --rm -it -v ${PWD}/env/mininet:/mininet \
		-v ${PWD}/../common/tests:/root/tests -v ${PWD}/env/mininet/tmp/${MN_STR_NAME}:/tmp \
		-v ${PWD}/../common/tests/pdu_types.py:/root/pdu_types.py \
		-v ${PWD}/../common/tests/send_efcp_h1.py:/root/send_efcp_h1.py \
		-v ${PWD}/../common/tests/send_efcp_h2.py:/root/send_efcp_h2.py -p50001:50001 \
		--name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} sergiogimenez/mininet-mod:latest
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

edf-dp-test-stratum: efcp-v1model-build
	make env-v1model-restart
	@rm -rf ${PWD}/test && mkdir -p ${PWD}/test
	@sleep 1
	@echo "Starting test for host1"
	make edf-dp-test-stratum-send-pdu iface="eth1" src_host="h1" dst_host="h2"
	@echo "Starting test for host2"
	make edf-dp-test-stratum-send-pdu iface="eth2" src_host="h2" dst_host="h1" 
	@sleep .5
	-sudo chown ${USER}:${USER} ${PWD}/env/mininet/tmp -R
	make env-v1model-stop

edf-dp-test-stratum-send-pdu:
	@touch ${PWD}/test/stratum_bmv2_test_${src_host}.log
	make edf-dp-test-stratum-send-pdu-type iface="${iface}" src_host="${src_host}" dst_host="${dst_host}" pdu="efcp" count="2"
	make edf-dp-test-stratum-send-pdu-type iface="${iface}" src_host="${src_host}" dst_host="${dst_host}" pdu="ipv4" count="1"
	@test -s ${PWD}/test/stratum_bmv2_test_${src_host}.log && echo "Test for host ${src_host} passed" || \
		echo "Test for host ${src_host} did not pass"

edf-dp-test-stratum-send-pdu-type:
	@docker exec -d ${MN_STR_NAME} tcpdump -i s1-${iface} -c ${count} -w output_${iface}
	@docker exec ${MN_STR_NAME} m ${src_host} python scripts/${src_host}_send_${pdu}_${dst_host}.py
	@sleep 1
	@echo "${pdu} test" >> ${PWD}/test/stratum_bmv2_test_${src_host}.log
	@docker exec ${MN_STR_NAME} tcpdump -XX -r output_${iface} >> ${PWD}/test/stratum_bmv2_test_${src_host}.log

edf-dp-test-tna-bfmodel-clean:
	@echo "Terminating any running Tofino model or switch instance"
	@${PWD}/test/arch/tna/tofino_model_terminate.sh
	@${PWD}/test/arch/tna/tofino_switch_terminate.sh

edf-dp-test-tna-bfmodel: efcp-tna-build edf-dp-test-tna-bfmodel-clean
	@echo "Copying test files"
	@cp -Rp ${PWD}/test/arch/tna/*.py ${EDF_DP_BUILD_ARCH_TNA}/
	@echo "Running Tofino model"
	${SDE}/run_tofino_model.sh -p ${EDF_DP_P4_TNA_NAME} > /dev/null 2>&1 &
	@sleep 30
	@echo "Running Tofino switch"
	${SDE}/run_switchd.sh -p ${EDF_DP_P4_TNA_NAME} > /dev/null 2>&1 &
	@sleep 60
	@echo "Running ${EDF_DP_P4_TNA_NAME} test"
	# Target fails if test fails
	${SDE}/run_p4_tests.sh -p ${EDF_DP_P4_TNA_NAME} 2>&1
	@make edf-dp-test-tna-bfmodel-clean
