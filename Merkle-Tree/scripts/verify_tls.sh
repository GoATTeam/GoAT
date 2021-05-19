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

######## TLS transcript verification ########

if ! test -f "proofs/phase1.out"; then
    echo "proofs/phase1.out not present"
    exit
fi

if ! test -f "scripts/verify_tls_signatures.py"; then
    echo "scripts/verify_tls_signatures.py not present"
    exit
fi

printf "=====TLS transcript verification begin=====\n"

i=0
while IFS= read -r line; do
    if [ $i -eq 0 ]; then
        seed=$line
        first=${seed:0:8}
        case=${first^^}
        msg="proofs/MSG_TLS_"$case".out"
        sig="proofs/SIG_TLS_"$case".out"
        pk_der="proofs/PK_TLS_"$case".der"
        pk_pem="proofs/PK_TLS_"$case".pem"
        if ! test -f $msg || ! test -f $sig || ! test -f $pk_der; then
            echo "Check if "$msg", "$sig" and "$pk_der" exist"
            exit
        fi
        # First convert der to pem
        echo "Running openssl pkey -inform DER -pubin -in "$pk_der" -out "$pk_pem
        openssl pkey -inform DER -pubin -in "$pk_der" -out "$pk_pem"
        # Now run signature verification
        echo "Running python3 verify_tls_signatures.py "$pk_pem $msg $sig
        out=$(python3 scripts/verify_tls_signatures.py "$pk_pem" "$msg" "$sig")
        if test "$out" != "Signature is valid."; then
            echo "Signature verification failed"
            exit
        fi
        i=1
    else
        i=0
    fi
done < proofs/phase1.out

printf "=====TLS transcript verification succeeds=====\n\n"

######## Chaining verification ########

i=0
clientrand=
sighash=

printf "=====Chaining verification begin=====\n"
while IFS= read -r line; do
    if [ $i -eq 0 ]; then
        i=1
        clientrand=$line
        first=${clientrand:0:8}
        case=${first^^}
        msg="proofs/MSG_TLS_"$case".out"
        echo "Checking first 32 bytes of "$msg
        first32=$(xxd -l32 -p -c32 "$msg")
        if test $clientrand != $first32; then
            echo "Chaning problem in client_rand actual("$first32") expected("$clientrand")"
            exit
        fi
    else
        i=0
        sighash=$line
        first=${clientrand:0:8}
        case=${first^^}
        sig="proofs/SIG_TLS_"$case".out"
        echo "Computing hash of "$sig
        acthash=$(sha256sum "$sig" | awk -F' ' '{print $1}')
        if test $acthash != $sighash; then
            echo "Chaning problem with sig hashes actual("$acthash") expected("$sighash")"
            exit
        fi
    fi
done < proofs/phase1.out

printf "=====Chaining verification succeeds=====\n"

# In practice, you should also check equivalence between phase1.out and phase2.out
