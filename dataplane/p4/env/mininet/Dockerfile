LABEL maintainer=Stratum dev <stratum-dev@lists.stratumproject.org>
LABEL description=Docker-based Mininet image that uses stratum_bmv2 as the default switch

ENV RUNTIME_DEPS=iproute2\
iputils-ping\
net-tools\
ethtool\
socat\
psmisc\
procps\
iperf\
arping\
telnet\
python-pexpect\
tcpdump\
screen\
libboost-filesystem1.62.0\
libboost-program-options1.62.0\
libboost-thread1.62.0\
libjudydebian1\
libgmp10\
libpcap0.8

/bin/sh -c install_packages $RUNTIME_DEPS

COPY dir:a215152032c3ef2d3cb56fa7edb8a3bf5350e3fb89a1c2210ccc3eafb245ec88 in / 

/bin/sh -c ldconfig

WORKDIR /root

COPY file:c9dffdc122eba901b4e7f4e46cbaf0dc4bd6acb03b45c0b6b1a59e47265bff6e in ./dummy.json 
COPY file:e9788f0fe60c58171e33a3dee4c77ad2bb4e5a0c4380bd1eba2414a9b538dbed in ./stratum.py 

ENV PYTHONPATH=:/root

EXPOSE 50001/tcp 50002/tcp 50003/tcp 50004/tcp 50005/tcp 50006/tcp 50007/tcp 50008/tcp 50009/tcp 50010/tcp 50011/tcp 50012/tcp 50013/tcp 50014/tcp 50015/tcp 50016/tcp 50017/tcp 50018/tcp 50019/tcp 50020/tcp 50021/tcp 50022/tcp 50023/tcp 50024/tcp 50025/tcp 50026/tcp 50027/tcp 50028/tcp 50029/tcp 50030/tcp 50031/tcp 50032/tcp 50033/tcp 50034/tcp 50035/tcp 50036/tcp 50037/tcp 50038/tcp 50039/tcp 50040/tcp 50041/tcp 50042/tcp 50043/tcp 50044/tcp 50045/tcp 50046/tcp 50047/tcp 50048/tcp 50049/tcp 50050/tcp 50051/tcp 50052/tcp 50053/tcp 50054/tcp 50055/tcp 50056/tcp 50057/tcp 50058/tcp 50059/tcp 50060/tcp 50061/tcp 50062/tcp 50063/tcp 50064/tcp 50065/tcp 50066/tcp 50067/tcp 50068/tcp 50069/tcp 50070/tcp 50071/tcp 50072/tcp 50073/tcp 50074/tcp 50075/tcp 50076/tcp 50077/tcp 50078/tcp 50079/tcp 50080/tcp 50081/tcp 50082/tcp 50083/tcp 50084/tcp 50085/tcp 50086/tcp 50087/tcp 50088/tcp 50089/tcp 50090/tcp 50091/tcp 50092/tcp 50093/tcp 50094/tcp 50095/tcp 50096/tcp 50097/tcp 50098/tcp 50099/tcp 50100/tcp

ENTRYPOINT ["mn" "--custom" "/root/stratum.py" "--switch" "stratum-bmv2" "--host" "no-offload-host" "--controller" "none"]
