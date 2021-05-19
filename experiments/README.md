# Experiments

## Measurement study results

We conduct a small initial measurement study. The results of which are categorized below based on the two types of anchors used. 

### TLS 1.2

1. `results/local.csv` contains domain names that belong to educational institutions. Filtered if they have .edu as their tld or .ac as their sld

2. `results/local-filtered.csv` contains the sublist from the first 2852 domains in `local.csv` that return correct time

3. `results/anchors.csv` contains those that are not backed by a CDN provider -> 298 domains remain

### Roughtime

Anchor owner | Name | IP (Location) | RTT AWS-CA | RTT AWS-NV | RTT AWS-SP 
------------ | ---- | ------------- | --------- | ---------- | ---------
Google | roughtime.sandbox.google.com | 173.194.206.158 (Virginia) | 26-27ms | 10-11ms | 155ms
Cloudflare | roughtime.cloudflare.com | 162.159.200.1 (SF) | 30-33ms | 17-18ms | 30ms
Chainpoint | roughtime.chainpoint.org | 35.194.78.198 (Washington DC) | ? | 3.4-3.5ms | 208ms 
int08h | roughtime.int08h.com | 35.192.98.51 (Iowa) | ? | 30ms | 190ms 

## Scripts

- `time_rsa_filter.sh` to find domains that return correct time and support RSA authentication

- `iploc_keycdn.py` to find ISP information (uses ip2location's DB) and `iploc_ip2location.py` to fetch IP location information

- `rtt_openssl.sh` and `rtt_tlse.sh` to find round trip times

## Organization

Data inside `results` is organized as follows:

- `exp1`: Finding anchors close to three AWS locations: London, North Virginia(NV), Singapore
- `exp2`: Observing public keys used by TLS servers
- `exp3`: Measuring TLS RTT from AWS-NV to close anchors over a period of time 
- `exp4`: Running `GeoCommit` from AWS-NV and AWS-Lon for over a week with 10 selected anchors
