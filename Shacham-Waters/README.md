# Shacham-Waters PoRet

Installation instructions:

> sudo apt install gcc make libsodium-dev
> bash scripts/install/install-pbc.sh
> bash scripts/install/install-roughtime.sh (Optional. Needed only if a Roughtime anchor is being used.) 

The default curve used in GoAT is `g149.param`. Some other curves copied from PBC libary can be found inside the `param` folder. 

Most binaries provide a list of command-line options with the `-h` flag.

1. `./create`: Creates all necessary data and places it in the `data` folder. Generated data include a file of specified size, key pair, tag and bases for Pedersen commitment. The `swpor` binary can be used to debug the PoRet implementation.
2. `./commit`: Runs the `GeoCommit` protocol. Anchor name, amplification factor can be specified. Output written to `proofs/phase1.out`.
3. `./prove_and_verify`: Runs both the `Prove` and `Verify` protocols using information in `proofs/phase1.out`.

## Implementation Notes

1. Note that the code to support PoRet proof aggregation over multiple geo-commit instances is not currently implemented, although it should be trivial to do so

2. Setup (`create`) takes long time if the file size specified is big. It is recommended to run it on smaller file sizes only. Opportunities to fasten it  via parallelization exist.

3. File alignment requirements arising due to the use of libaio mean that a change of file size needs to be done carefully. The simplest approach is to change the number of blocks `NUM_BLOCKS` (NOT other parameters in `include/common.h`).