#!/bin/bash

#while IFS=, read -r col1 col2
for i in {0..2}
do
    IFS=, read -r col1 col2 col3
    for j in {0..1}
    do
        domain=$col2
        if [ $j -eq 1 ]; then
            domain="www."$domain
        fi
        IFS= x509=$(echo | timeout --preserve-status 15 openssl s_client -connect "$domain":443 -tls1_2 -cipher aRSA 2>/dev/null |  
                    openssl x509 -text)
        if (( ! $? )); then
            if ! test -z "$x509"; then
                hash=$(echo $x509 | sha256sum | cut -d' ' -f1) 
                serial=$(echo $x509 | grep -A1 "Serial" | tail -n1 | tr -s ' ')
                echo "$domain complete"
                echo "$col1,$col2,$col3,$hash,$serial" >> publickeys.csv
            else
                echo "$col1 $col2 ERROR2 on trying host, skipping"
            fi
        else
            echo "$col1 $col2 ERROR on trying host, skipping"
        fi
    done
done < timeservers.csv