#!/bin/bash

# Filtering the ones that a) return right time and b) work with RSA auth
# IMPORTANT NOTE: Make sure tlse.c does not have any other printed output except time in seconds

TLSE_PATH="./tlse/"
TLSE="$TLSE_PATH"tlstime

if ! test -f "$TLSE"; then
    echo "tlse does not exist"
    exit
fi

#while IFS=, read -r col1 col2
for i in {0..200}
do
    IFS=, read -r col1 col2
    for j in {0..1}
    do
        domain=$col2
        if [ $j -eq 1 ]; then
            domain="www."$domain
        fi
        CURTIME=$(date +%s)
        TLSDATE_DATETIME=$(timeout --preserve-status 15 "$TLSE" "$domain" 443 5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2 1)
        if (( ! $? )); then
            if ! test -z "$TLSDATE_DATETIME"; then
                echo "$col1 $domain $TLSDATE_DATETIME"
                TIMEDIF=$(expr $TLSDATE_DATETIME - $CURTIME)
                if (( $TIMEDIF < 10 && $TIMEDIF > -10 )); then
                    echo "$col1,$domain" >> accepted.csv
                fi
            else
                echo "$col1 $domain ERROR2 on trying host, skipping"
            fi
        else
            echo "$col1 $domain ERROR on trying host, skipping"
        fi
    done
done < uk.csv