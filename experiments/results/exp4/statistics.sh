#!/bin/bash


# if test -d $1; then
#     echo "Usage: bash statistics.sh <DIR>"
#     exit
# fi

IFS=
results=$(tail -n1 "$1"/* | grep RESULT | awk -F' ' '{print $2, $4}')

# echo $results
retries=""
fail=0
count=0

while IFS= read -r line; do
    IFS=' ' tokens=( $line )
    # echo ${tokens[0]} ${tokens[1]}
    if [[ ${tokens[0]} == 1 ]]; then
        retries+=$(expr ${tokens[1]} - 1)
        retries+="\n"
    else
        fail=$(expr $fail + 1)
    fi
    count=$(expr $count + 1)
done <<< "$results"

mean=$(
    printf $retries |
        awk '{sum+=$1}END{print (sum/NR)}'
)

standardDeviation=$(
    printf $retries |
        awk '{sum+=$1; sumsq+=$1*$1}END{print sqrt(sumsq/NR - (sum/NR)*(sum/NR))}'
)
echo "Fail: $fail, Total: $count, Mean: $mean, SD: $standardDeviation"
