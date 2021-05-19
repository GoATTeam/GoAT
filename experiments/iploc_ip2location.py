import os, IP2Location
import pandas as pd
import socket

# Does not have ISP
db = "../data/ip2location_db5/IP2LOCATION-LITE-DB5.BIN"
database = IP2Location.IP2Location(db)

anchors_df = pd.read_csv('results/uk-final.csv')
ip2location_csv = "results/location/ip2location.csv"
keycdn_csv = "results/location/keycdn.csv"
keycdn_df = pd.read_csv(keycdn_csv).set_index('domain')


f = open(ip2location_csv, "a")

I = 33333333

for row in anchors_df.itertuples(index=False):
    I = I + 1
    domain_name = row.name 
    try:
        ipaddr = socket.gethostbyname(domain_name)
        if (domain_name in keycdn_df.index and 
            (abs(database.get_latitude(ipaddr) - keycdn_df.at[domain_name, 'lat']) > 2 or abs(database.get_longitude(ipaddr) - keycdn_df.at[domain_name, 'lon']) > 2)):
            print("===========")
            print(domain_name, database.get_latitude(ipaddr), database.get_longitude(ipaddr))
            print(keycdn_df.at[domain_name, 'lat'], keycdn_df.at[domain_name, 'lon'])
        print([I, domain_name, database.get_latitude(ipaddr), database.get_longitude(ipaddr), ipaddr])
        f.write(str(I) + "," + 
            domain_name + "," + 
            str(database.get_latitude(ipaddr)) + "," + 
            str(database.get_longitude(ipaddr)) + "," + 
            ipaddr + "\n")
    except (socket.gaierror):
        print("Skipping", domain_name)

# pd.DataFrame(l, columns=['id', 'domain', 'lat', 'lon', 'ip']).to_csv(ip2location_csv, index=False)
