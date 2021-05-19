#!/bin/bash


if test -z $1; then
    echo "Usage: bash statistics.sh n_rounds"
    exit
fi

if ! test -f "scripts/amplify_gen_full_proof.sh"; then
    echo "proofs/phase1.out not present"
    exit
fi

times=""
for i in {0..2}
do
    times+=$(bash scripts/amplify_gen_full_proof.sh $1 | grep Total | awk -F' ' '{print $2}')
    times+="\n"
done


echo $times

mean=$(
    printf $times |
        awk '{sum+=$1}END{print (sum/NR)}'
)

standardDeviation=$(
    printf $times |
        awk '{sum+=$1; sumsq+=$1*$1}END{print sqrt(sumsq/NR - (sum/NR)*(sum/NR))}'
)
echo "Mean: $mean, SD: $standardDeviation"
