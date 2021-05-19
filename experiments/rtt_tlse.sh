#!/bin/bash

# Uses tlse to measure the rtt

NUM_REP=10
TLSE_PATH="./tlse/"
TLSE="$TLSE_PATH"threads
NONCE="5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2"

if ! test -f "$TLSE"; then
    echo "tlse does not exist"
    exit
fi

INPUT=anchors.csv

i=0
while IFS=, read -r col1 col2 col3
do
    if [ $i -eq 0 ]; then
	echo $col1 $col2 $col3
	i=1
        continue
    fi
    TLSTIME=
    TLSPINGTIME=
    TLSDATE_DATETIME=$(timeout --preserve-status 15 "$TLSE" "$col2" "$NUM_REP" 2>/dev/null)
    if (( ! $? )); then
        if ! test -z "$TLSDATE_DATETIME"; then
	    TLSTIME=$(echo "$TLSDATE_DATETIME" | tail -n1)
	    TLSPINGTIME=$(echo "$TLSDATE_DATETIME" | tail -n2 | head -n1)
            echo $col1 $col2 $TLSPINGTIME $TLSTIME 
        else
            echo "$col1 $col2 ERROR2 on trying host, skipping"
            continue
        fi
    else
        echo "$col1 $col2 ERROR1 on trying host, skipping"
        continue
    fi
    echo $TLSDATE_DATETIME
    echo "$col1,$col2,$TLSTIME,$TLSPINGTIME" >> rtt.csv
done < "$INPUT"
