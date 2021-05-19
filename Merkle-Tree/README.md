# Merkle Tree PoRet

> Install: sudo apt install gcc make libsodium-dev python3-pip; pip3 install pycryptodome

The POR only works for block sizes in powers of 2. Necessary code to generate such a file is provided. For debugging purposes, each file block contains the corresponding block index. Most binaries provide a list of command-line options with the `-h` flag.

1. `./create`: Generates two files `FILE` and `TAG`. The size of the former needs to be specified. The latter contains the Merkle tree hashes. 
2. `./commit <seed>`: On input a 32 byte seed, a commitment `com1` to the PoR proof is generated. Anchor name, amplification factor can be specified.
3. `./prove <seed>`: Generate the full proof `PROOF` and denote `com2 = H(proof)`.
4. `./verify_por <seed> <com1> <com2>`: Success / Failure.

Notes: 

1. For alignment purpose, each hash in the tag file sits at a byte index multiple of 512, i.e., 512 / 32 = 8x blowup in the tag file size. Requirement due to use of libaio.

2. The proof is ~2x larger because it contains both the sibling and the path to the root.

## Usage

1. `./commit -a anchor -n n_rounds`: Amplifies `n_rounds` times. Writes PoR commitment, anchor signature into `proofs/phase1.out`.
2. `scripts/amplify_gen_proof_full.sh n_rounds`: Generate full PoR proofs, writes output to `proofs/phase2.out`.
3. `scripts/verify_tls.sh`, `scripts/verify_roughtime.sh`: Verifies PoR proofs, transcript signatures and the chaining.
