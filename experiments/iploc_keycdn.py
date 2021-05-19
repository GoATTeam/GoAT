import pandas as pd
import urllib3

prohibited = ["MICROSOFT", "CLOUDFLARE", "GOOGLE", "AMAZON", "EDGE-HOSTING"] # CDNs

def is_prohibited(isp):
    for i in prohibited:
        if isp.lower() in i.lower() or i.lower() in isp.lower():
            return True
    return False

table1 = pd.read_csv('results/uk-filter1.csv', names=['id', 'domain'])
domains = list(table1['domain'])

import requests
keycdn_csv = "results/location/keycdn.csv"

headers = {"user-agent": "keycdn-tools:https://example.com"}
base_url = "https://tools.keycdn.com/geo.json?host="


table2 = pd.read_csv(keycdn_csv)
isps = table2.set_index('domain')['isp']
table2.set_index('domain')

f = open("results/uk-final.csv", "a")
f.write("name\n")

for domain in domains:
    print("=======")
    if table2.get(domain) != None:
        print("Skipping API call")
        print(table2.loc[table2['domain'] == domain])
        if (is_prohibited(isps[domain])):
            print("A CDN..")
        continue
    print("Making API call for", domain)
    url = base_url + domain
    try:
        r = requests.get(url, headers=headers, timeout=10)
        if (r.status_code != 200):
            print("Failed 1")
            continue
        json = r.json()
        if (json['status'] != 'success'):
            print("Failed 2")
            continue
        data = json['data']['geo']
        isp = data['isp']
        country_name = data['country_name']
        print(domain, country_name, isp, json['data']['geo'])
        # f.write(domain + "," + str(isp.replace(',', '-')) + "," + str(data['latitude']) + "," + 
        #         str(data['longitude']) + "," + str(data['ip']) + "," + str(data['region_name']) + "," +
        #         str(data['city']) + "," + str(data['country_name']) + "\n")
        if (is_prohibited(isp)):
            print("A CDN..")
        else:
            print("Writing!")
            f.write(domain + "\n")
    except (urllib3.exceptions.ReadTimeoutError, requests.exceptions.ReadTimeout):
        print("Timed out")