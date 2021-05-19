#!/bin/bash 


CUR_DIR=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P)
FOLDER_PATH="$CUR_DIR/../Merkle-Tree/"
BINARY="commit"
NUM_RETRIES=30
GAP=1 # sleeps one second between retries

if ! test -f "$FOLDER_PATH$BINARY"; then
    echo "commit does not exist"
    exit
fi

if test -z $1 || test -z $2; then
    echo "Usage: $0 ANCHOR_NAME DELTA_IN_MS (Both TLS & Roughtime supported.)"
    exit
fi

set -e

actual_amp=$(python -c "from math import floor; print int(floor(1000.0/$2) - 1)")
set_amp=$(python -c "from math import ceil; print int(2*ceil(1000.0/$2) - 1)")

type=0
amp_factor=0
if [[ $1 == roughtime* ]]; then
    echo "Roughtime anchor: $1"
    type=2
    amp_factor=1
else
    echo "TLS anchor: $1"
    type=1
    amp_factor=$set_amp
    echo "Actual: $actual_amp, Set: $set_amp"
fi

cur_time=$( date )
echo "Beginning execution at "$cur_time

global_success=0
num_tries=0
for j in $(seq 1 $NUM_RETRIES);
do
    IFS=
    output=$( cd "$FOLDER_PATH" ; ./"$BINARY" -t "$type" -a "$1" -n "$amp_factor")
    echo $output

    uniq_out=$(echo "$output" | grep "time:" | uniq -c)
    echo "$uniq_out"

    success=0
    if [[ $type == 1 ]]; then # TLS
        while read -r line
        do
            count=$(echo "$line" | xargs | cut -d' ' -f1)
            echo "Count is $count"
            if test "$count" -ge "$actual_amp"; then
                success=1
            fi
        done <<< "$uniq_out"
    else # Roughtime
        tot_lines=$(echo "$uniq_out" | wc -l)
        if [[ $tot_lines != 2 ]]; then
            echo "ERROR!!"
        fi
        t1=$(echo "$uniq_out" | head -n1 | xargs | cut -d' ' -f5)
        t2=$(echo "$uniq_out" | tail -n1 | xargs | cut -d' ' -f5)
        diff=$(expr $t2 - $t1)
        echo "Diff is $diff"
        check=$(python -c "print $diff <= $2")
        if [[ $check == "True" ]]; then
            success=1
        fi
    fi

    echo "success: $success"
    if [[ $success == 1 ]]; then
        global_success=1
        num_tries=$j
        break
    else
        echo "Sleeping before retrying"
        sleep $GAP
    fi
done

echo "RESULT: $global_success in $num_tries tries"
