#pragma once

#include <stdint.h>

#define SS 32
#define NUM_SECTORS 96 // Choose 16 multiples 
#define BS (SS * NUM_SECTORS) // 96 * 32 = 3072
#define NUM_BLOCKS 128
#define FS (BS * NUM_BLOCKS)

#define QUEUE_DEPTH 32
#define NUM_CHAL 100

#define DEFAULT_DATA_FILE "data/file.dat"
#define DEFAULT_TAG_FILE "data/tag.dat"
#define DEFAULT_SK_FILE "data/sk.dat"
#define DEFAULT_PK_FILE "data/pk.dat"
#define DEFAULT_BASES_FILE "data/bases.dat"
#define DEFAULT_P1_OUT "proofs/phase1.out"
#define DEFAULT_P2_OUT "proofs/phase2.out"

#define G1_LEN_COMPRESSED 49
#define G2_LEN_COMPRESSED 97

#define TLS_1_2_ANCHOR 1
#define ROUGHTIME_ANCHOR 2

#define ANCHOR_SIGN_SIZE 32
#define POR_COMM_SIZE G1_LEN_COMPRESSED
