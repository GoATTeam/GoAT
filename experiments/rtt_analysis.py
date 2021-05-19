import pandas as pd

# sort -k3 -n -t, CSV

prohibited = ["MICROSOFT-CORP-MSN-AS-BLOCK", "CLOUDFLARENET", "GOOGLE", "AMAZON-AES", "EDGE-HOSTING"] # CDNs

table1 = pd.read_csv('results/exp1/rtt-nv-1612359349.csv')
table2 = pd.read_csv('results/exp1/rtt-sp-1612269306.csv')
table3 = pd.read_csv('results/locations.csv')

rtt_nv = table1.set_index('domain')['rtt'].to_dict()
rtt_sp = table2.set_index('domain')['rtt'].to_dict()
isp = table3.set_index('domain')['isp']

domains = list(table2['domain'])
domains = [x for x in domains if isp.get(x) != None]

cdn_backed = [dom for dom in domains if isp[dom] in prohibited]

non_unique = [dom for dom in domains if rtt_nv[dom] < 600 and rtt_sp[dom] < 600]

unique = [dom for dom in domains if dom not in non_unique and isp[dom] not in prohibited]

print("cdn_backed(" + str(len(cdn_backed)) + ")", cdn_backed)

print()
print("non_unique(" + str(len(non_unique)) + ")", non_unique)

assert(len(set(non_unique).difference(set(cdn_backed))) == 0)

print()
print("unique(" + str(len(unique)) + ")", unique)

print()
print("=====Servers <100ms to AWS NV=====")
close_to_nv = []
for x in unique:
    if rtt_nv.get(x) != None and rtt_nv[x] < 100:
        close_to_nv.append(x)

print([(x, rtt_nv[x]) for x in sorted(close_to_nv, key=lambda x: rtt_nv[x])])

print()
print("=====Servers <100ms to AWS SP=====")
close_to_sp = []
for x in unique:
    if rtt_sp.get(x) != None and rtt_sp[x] < 100:
        close_to_sp.append(x)

print([(x, rtt_sp[x]) for x in sorted(close_to_sp, key=lambda x: rtt_sp[x])])

print()
print("=====Servers <100ms to AWS CA=====")

table4 = pd.read_csv('results/exp1/rtt-ca-1612361197.csv')
rtt_ca = table4.set_index('domain')['rtt'].to_dict()

close_to_ca = []
for x in unique:
    if rtt_ca.get(x) != None and rtt_ca[x] < 100:
        close_to_ca.append(x)

print([(x, rtt_ca[x]) for x in sorted(close_to_ca, key=lambda x: rtt_ca[x])])
