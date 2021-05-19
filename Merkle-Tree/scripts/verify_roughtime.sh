#!/bin/bash

######## POR proof verification ########

if ! test -f "proofs/phase2.out"; then
    echo "proofs/phase2.out not present"
    exit
fi

# -x verify_por
printf "=====POR proof verification begin=====\n"

while IFS=, read -r seed com1 com2; do
    echo "Running ./verify_por -r "$seed" -1 "$com1" -2 "$com2""
    out=$(IFS=' ' ./verify_por -r "$seed" -1 "$com1" -2 "$com2")
    success=$(echo "$out" | grep 'succeeds')
    if test -z "$success"; then
        echo "Operation failed. Output below"
        echo "$out"
        exit
    fi
done < proofs/phase2.out

printf "=====POR proof verification succeeds=====\n\n"

######## Roughtime transcript verification ########

if ! test -f "proofs/phase1.out"; then
    echo "proofs/phase1.out not present"
    exit
fi

rt_verify="../deps/roughenough/target/release/roughenough-verify"

if ! test -x "$rt_verify"; then
    echo $rt_verify" not present"
    exit
fi

if test -z $1; then
    echo "Usage: bash verify_roughtime.sh pk"
    exit
fi

printf "=====Roughtime transcript verification begin=====\n"

zeroes="0000000000000000000000000000000000000000000000000000000000000000"
i=0
sig=
while IFS= read -r line; do
    if [ $i -eq 0 ]; then
        seed=$line
        first=${seed:0:8}
        case=${first^^}
        transcript="proofs/TRANSCRIPT_RT_"$case".out"
        if ! test -f $transcript; then
            echo $transcript" does not exist"
            exit
        fi
        # Now run signature verification
        echo "Verifying" "$transcript"
        out=$("$rt_verify" "-c" "$seed""$zeroes" "-t" "$transcript" "-p" "$1")
        if test "$out" != "true"; then
            echo "Signature verification failed. Binary output follows:"
            echo $out
            exit
        fi
        sig=$(xxd -p -s40 -l32 -c32 "$transcript")
        i=1
    else
        if test $sig != $line; then
            echo $sig $line
            echo "Chaining problem = Signature is not used as PoR challenge"
            exit
        fi
        i=0
    fi
done < proofs/phase1.out

printf "=====Roughtime transcript verification succeeds=====\n\n"

# In practice, you should also check equivalence between phase1.out and phase2.out
