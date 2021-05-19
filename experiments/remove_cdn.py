import pandas as pd

table1 = pd.read_csv('results/local-filtered.csv', names=['id', 'domain'])
table2 = pd.read_csv('results/locations.csv')

criterion = table1['domain'].map(lambda x: locations.get(x) != None and is_prohibited(locations[x]) == False)