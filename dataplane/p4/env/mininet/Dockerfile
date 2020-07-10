FROM opennetworking/mn-stratum:latest

ENV EXTRA_RUNTIME_DEPS="python-pip tshark vim"

RUN apt-get update --allow-unauthenticated
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y $EXTRA_RUNTIME_DEPS
RUN pip install scapy

#RUN cp -p /root/stratum.py /root/mn_stratum.py
ADD ./mn_stratum.py /root/mn_stratum.py
ADD ./edf_stratum.py /root/stratum.py
#ADD ./topo.py /root/
ENTRYPOINT python /root/stratum.py
#ENTRYPOINT mn --custom /root/edf_stratum.py --switch stratum-bmv2 --host no-offload-host --controller none
##ENTRYPOINT ["mn" "--custom" "/root/edf_stratum.py" "--switch" "stratum-bmv2" "--host" "no-offload-host" "--controller" "none"]

#ADD ./mn_stratum.py /root/stratum.py
#ADD ./topo.py /root/
#ENTRYPOINT python /root/topo.py
