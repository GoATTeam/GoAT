#!/bin/bash

# Uses openssl s_client to measure the rtt

NUM_REP=10
echo $NUM_REP

for i in {0..297}
do
    IFS=, read -r col1 col2
    if [ $i -eq 0 ]; then
        continue
    fi
    TOTAL=0
    for j in $(seq 1 $NUM_REP); #number of repetitions
    do
        BENCHMARK_NANO_BEFORE=$(date +%s%N)
        TLSDATE_DATETIME=$(echo | timeout --preserve-status 15 openssl s_client -connect "$col2":443 -tls1_2 -cipher aRSA 2>/dev/null)
        if (( ! $? )); then
            if ! test -z "$TLSDATE_DATETIME"; then
                BENCHMARK_NANO_AFTER=$(date +%s%N)
                BENCHMARK=$(expr $BENCHMARK_NANO_AFTER - $BENCHMARK_NANO_BEFORE)
                echo $j $BENCHMARK
                TOTAL=$(expr $TOTAL + $BENCHMARK)
            else
                echo "$col1 $col2 ERROR2 on trying host, skipping"
                break
            fi
        else
            echo "$col1 $col2 ERROR1 on trying host, skipping"
            break
        fi
    done
    AVG=$(expr $TOTAL / $NUM_REP)
    BENCHMARK_TO_MILLI=$(echo "scale=6; $AVG.00/1000000" | bc)
    echo $col1 $col2 $BENCHMARK_TO_MILLI
    echo "$col1,$col2,$BENCHMARK_TO_MILLI" >> rtt.csv
done < data/local-filtered.csv