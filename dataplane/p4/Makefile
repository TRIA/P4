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
	cd env && docker-compose exec -d p4runtime mn-stratum rm /tmp/* || true && cd ..
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
	docker run --privileged --rm -it -v ${PWD}/env/mininet:/mininet \
		-v ${PWD}/../common/tests:/root/tests \
		-v ${PWD}/env/mininet/tmp/${MN_STR_NAME}:/tmp \
		-p50001:50001 \
		--name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} sergiogimenez/mininet-mod:latest

mn-stop:
	docker rm -f ${MN_STR_NAME} || true
	@make docker_net_rm

edf-dp-test-stratum:
	make env-restart
	@rm -rf ${PWD}/test && mkdir -p ${PWD}/test
	@sleep 3
	@echo "Starting test for host1"
	make edf-dp-test-stratum-send-pdu iface="eth1" src_host="h1" dst_host="h2"
	@echo "Starting test for host2"
	make edf-dp-test-stratum-send-pdu iface="eth2" src_host="h2" dst_host="h1" 
	@sleep .5
	-sudo chown ${USER}:${USER} ${PWD}/env/mininet/tmp -R
	make env-stop

edf-dp-test-stratum-send-pdu:
	@touch ${PWD}/test/stratum_bmv2_test_${src_host}.log
	make edf-dp-test-stratum-send-pdu-type iface="${iface}" src_host="${src_host}" dst_host="${dst_host}" pdu="efcp"
	make edf-dp-test-stratum-send-pdu-type iface="${iface}" src_host="${src_host}" dst_host="${dst_host}" pdu="ipv4"
	@test -s ${PWD}/test/stratum_bmv2_test_${src_host}.log && echo "Test for host ${src_host} passed" || \
		echo "Test for host ${src_host} did not pass"

edf-dp-test-stratum-send-pdu-type:
	@docker exec -d ${MN_STR_NAME} tcpdump -i s1-${iface} -c 1 -w output_${iface}
	@docker exec ${MN_STR_NAME} m ${src_host} python scripts/${src_host}_send_${pdu}_${dst_host}.py
	@sleep 1
	@echo "${pdu} test" >> ${PWD}/test/stratum_bmv2_test_${src_host}.log
	@docker exec ${MN_STR_NAME} tcpdump -XX -r output_${iface} >> ${PWD}/test/stratum_bmv2_test_${src_host}.log
