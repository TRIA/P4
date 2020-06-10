P4C_IMG := opennetworking/p4c:stable
DOCKER_NET := rina-controlplane
MN_STR_NAME := mn-stratum

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

env-start: docker_net_create
	cd env && docker-compose up -d && cd ..

env-stop:
	cd env && docker-compose down -t0 && cd ..
	@make docker_net_rm

env-restart: reset env-start

mn-cli:
	$(info *** Attaching to Mininet CLI...)
	$(info *** To detach press Ctrl-Z (Mininet will keep running))
	-@docker attach --detach-keys "ctrl-z" ${MN_STR_NAME} || echo "*** Detached from Mininet CLI"

mn-attach:
	-@docker exec -it ${MN_STR_NAME} bash

mn-log:
	docker logs -f ${MN_STR_NAME}

reset: env-stop
	-rm -rf ./tmp

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

mn-single-start: docker_net_create
	docker run --privileged --rm -it -v /tmp/${MN_STR_NAME}:/tmp -p50001-50030:50001-50030 --name ${MN_STR_NAME} --hostname ${MN_STR_NAME} --network=${DOCKER_NET} opennetworking/${MN_STR_NAME}

mn-single-stop:
	docker rm -f ${MN_STR_NAME}
	@make docker_net_rm
