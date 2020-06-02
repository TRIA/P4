P4C_IMG := opennetworking/p4c:stable
P4C_SHA := sha256:8f9d27a6edf446c3801db621359fec5de993ebdebc6844d8b1292e369be5dfea

DOCKER_NET := rina-controlplane

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
curr_dir := $(patsubst %/,%,$(dir $(mkfile_path)))
curr_dir_sha := $(shell echo -n "$(curr_dir)" | shasum | cut -c1-7)

default:
	$(error Please specify a make target (see README.md))

docker_net_create:
	@echo "setup"
	@docker network create ${DOCKER_NET} || true

env-start: docker_net_create
	cd env && docker-compose up -d && cd ..

env-stop:
	cd env && docker-compose down -t0 && cd ..
	@docker network rm ${DOCKER_NET} || true

env-restart: reset env-start

mn-cli:
	$(info *** Attaching to Mininet CLI...)
	$(info *** To detach press Ctrl-Z (Mininet will keep running))
	-@docker attach --detach-keys "ctrl-z" $(shell docker-compose ps -q mininet) || echo "*** Detached from Mininet CLI"

mn-attach:
	-@docker exec -it mininet-p4 bash

mn-log:
	docker logs -f mininet-p4

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

mn-single: docker_net_create
	docker run --privileged --rm -it -v /tmp/mn-stratum:/tmp -p 50001:50001 --name "mininet-stratum" --network=${DOCKER_NET} ${MN_STRATUM_IMG}
