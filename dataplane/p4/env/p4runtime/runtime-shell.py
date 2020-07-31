import p4runtime_sh.shell as sh

sh.setup(
    device_id=1,
    grpc_addr='mn-stratum:50001',
    election_id=(0, 1), # (high, low)
    config=sh.FwdPipeConfig('p4src/build/p4info.txt', 'p4src/build/bmv2.json')
)

print("Connected to bmv2")

# TABLE ENTRIES

## EFCP
te = sh.TableEntry('MyIngress.efcp_lpm')(action = 'MyIngress.efcp_forward')
te.match['hdr.efcp.dstAddr'] = '1'
te.action['dstAddr'] = '00:00:00:00:00:01'
te.action['port'] = '1'
te.action['vlan_id'] = '0'
te.insert()

te = sh.TableEntry('MyIngress.efcp_lpm')(action = 'MyIngress.efcp_forward')
te.match['hdr.efcp.dstAddr'] = '2'
te.action['dstAddr'] = '00:00:00:00:00:02'
te.action['port'] = '2'
te.action['vlan_id'] = '0'
te.insert()

## IPv4
te = sh.TableEntry('MyIngress.ipv4_lpm')(action = 'MyIngress.ipv4_forward')
te.match['hdr.ipv4.dstAddr'] = '10.0.0.1'
te.action['dstAddr'] = '00:00:00:00:00:01'
te.action['port'] = '1'
te.insert()

te = sh.TableEntry('MyIngress.ipv4_lpm')(action = 'MyIngress.ipv4_forward')
te.match['hdr.ipv4.dstAddr'] = '10.0.0.2'
te.action['dstAddr'] = '00:00:00:00:00:02'
te.action['port'] = '2'
te.insert()

## EFCP Counter
te = sh.TableEntry('MyIngress.efcp_count_lpm')(action = 'MyIngress.tally_count_efcp')
te.match['hdr.efcp.dstAddr'] = '1'
te.insert()

te = sh.TableEntry('MyIngress.efcp_count_lpm')(action = 'MyIngress.tally_count_efcp')
te.match['hdr.efcp.dstAddr'] = '2'
te.insert()

## IPv4 Counter
te = sh.TableEntry('MyIngress.ipv4_count_lpm')(action = 'MyIngress.tally_count_ipv4')
te.match['hdr.ipv4.dstAddr'] = '10.0.0.1'
te.insert()

te = sh.TableEntry('MyIngress.ipv4_count_lpm')(action = 'MyIngress.tally_count_ipv4')
te.match['hdr.ipv4.dstAddr'] = '10.0.0.2'
te.insert()


print("Submitted configuration entries to bmv2")

connection = sh.client
while True:
    print("Waiting to receive something")
    packet = connection.stream_in_q.get()

    print("Packet received!:" + str(packet))
    connection.stream_out_q.put(packet)

sh.teardown()
