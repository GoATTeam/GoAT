#!/bin/bash

if ! test -f prove; then
    echo "Binary prove not present"
    exit
fi

if test -z $1; then
    echo "Usage: bash amplify_proof_gen.sh n_rounds"
    exit
fi

if ! test -f "proofs/phase1.out"; then
    echo "proofs/phase1.out not present"
    exit
fi

if test -f "proofs/phase2.out"; then
    echo "Removing proofs/phase2.out"
    rm proofs/phase2.out
fi

times=""
i=0
cnt=0
str=
seed=
com1=
com2=
while IFS= read -r line; do
    if [ $i -eq 1 ] && [ $cnt -lt $1 ]; then
        seed=$line
        echo "Running ./prove -r $seed"
        com2=$(./prove -r "$seed" | grep '(proof)' | awk -F' ' '{print $2}')
        times+=$(./prove -r "$seed" | grep '\[Time' | awk -F' ' '{print $10}')
        times+="\n"
        let "cnt++"
        i=0
    else
        i=1
        if ! test -z $seed ; then
            com1=$line
            echo $seed,$com1,$com2 >> proofs/phase2.out
            seed=
        fi
    fi
done < proofs/phase1.out

echo $times

sum=$(
    printf $times |
        awk '{sum+=$1}END{print sum}'
)

echo "Total: $sum"
