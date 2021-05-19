#!/bin/bash

if test -z $1; then
    echo "Usage: bash get_cert_pk.sh domain"
    exit
fi

domain=$1
out=$(tr '.' '_' <<<"$domain")
cert=tlscerts/"$out"_cert.pem
pk=tlspubkeys/"$out"_pk.pem

if test -f $cert && test -f $pk; then
    echo "Already fetched"
    exit
fi


echo | openssl s_client -connect $domain:443 -tls1_2 -cipher aRSA 2>/dev/null | openssl x509 -out $cert

echo "Certificate written to "$cert

openssl x509 -in $cert -pubkey -noout -out $pk

echo "Public key written to "$pk

# openssl x509 -in tlscerts/www_westminster_edu_cert.pem -serial -noout