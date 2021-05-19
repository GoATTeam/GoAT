#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "sodium.h"
#include "common.h"

struct conf {
    char* seed;
    char *com1, *com2;
    size_t file_size;
	size_t block_size;
	int nops;
};

static void print_help(FILE *f, char *p) {
	fprintf(f,
            "Usage: %s -r seed -1 com1 -2 com2 -s file_size\n",
            p);
}

static void
parse_conf(struct conf *cnf, int argc, char *argv[]) {

	int opt;
	while ((opt = getopt(argc, argv, "hs:r:1:2:")) != -1) {
		switch (opt) {
			case 's':
			cnf->file_size = atoi(optarg);
			break;

			case 'r':
			cnf->seed = optarg;
			break;

            case '1':
            cnf->com1 = optarg;
            break;

            case '2':
            cnf->com2 = optarg;
            break;

			case 'h':
			print_help(stdout, argv[0]);
			exit(0);

			default:
			print_help(stderr, argv[0]);
			exit(1);
		}
	}
}

/**
 * Proof structure:
 * 
 * for chal in 1..x:
 *     write(file_block)
 *     for level in y...0:
 *          write(merkle_tree[level][0] || write(merkle_tree[level][1]))

 * Note that merkle_tree[0][0] = merkle_tree[0][1] = root
 */
int main(int argc, char* argv[]) {

    struct conf cnf = {
        .seed = "5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2",
        .com1 = NULL,
        .com2 = NULL,
        .block_size = BS,
        .file_size = FS,
        .nops = NUM_CHAL
    };

    parse_conf(&cnf, argc, argv);

    if (strlen(cnf.seed) != 64 || !cnf.com1 || strlen(cnf.com1) != 64 || !cnf.com2 || strlen(cnf.com2) != 64) {
        printf("Seed and the two commitments must be 32 bytes and in hex.\n");
        print_help(stdout, argv[0]);
        return 0;
    }
    size_t file_size = cnf.file_size;
    size_t block_size = cnf.block_size;
    size_t num_blocks = MB(file_size) / block_size;
    int levels = 1 + log2(num_blocks);

    printf("file_size:%ldMB block_size:%ld "
            "num_chal:%d\nSeed: %s\nCom1: %s\nCom2: %s\nchals: %d\n", 
            file_size, block_size, cnf.nops, cnf.seed, cnf.com1, cnf.com2, cnf.nops);

    unsigned char seed_bytes[32];
    unsigned char expected_com1[32];
    unsigned char expected_com2[32];
    hex64_to_bytes(cnf.seed, seed_bytes);
    hex64_to_bytes(cnf.com1, expected_com1);
    hex64_to_bytes(cnf.com2, expected_com2);

    // Derive challenges
    uint32_t challenges[NUM_CHAL];
    randombytes_buf_deterministic(challenges, sizeof(challenges), seed_bytes);
    // printf("%u\n", challenges[2]);

    FILE *proof_file;
    char proof[50];
    int length = sprintf(proof, "proofs/PROOF_POR_");
    for (int i=0; i<4; i++)
        length += sprintf(proof + length, "%02x", seed_bytes[i]);
    length += sprintf(proof + length, ".out");
    proof[length] = '\0';

    printf("opening %s\n", proof);
    proof_file = fopen(proof, "r");

    unsigned char actual_com1[32], actual_com2[32], tmp[32];
    crypto_hash_sha256_state state1, state2;
    crypto_hash_sha256_init(&state1);
    crypto_hash_sha256_init(&state2);

    char buf[BS];
    char hash_string[65], hash_string2[65];
    uint8_t child_blocks[65];
    child_blocks[64] = '\0';
    uint8_t *hl = child_blocks, *hr = child_blocks + 32, *h;
    for (int i=0; i<NUM_CHAL; i++) {
        // printf("Chal %d\n", i);
        int block_num = mod(challenges[i], num_blocks);
        fread(buf, 1, block_size, proof_file);
        crypto_hash_sha256_update(&state1, buf, block_size);
        crypto_hash_sha256_update(&state2, buf, block_size);
        int cur = block_num;
        for (int j=levels-1; j>=0; j--) {
            // printf("Level %d %d\n", j, cur);
            if (j == levels - 1) {
                crypto_hash_sha256(tmp, buf, block_size);
            } else {
                crypto_hash_sha256(tmp, child_blocks, 64);
            }

            fread(hl, 1, 32, proof_file);
            fread(hr, 1, 32, proof_file);

            crypto_hash_sha256_update(&state2, hl, 32);
            crypto_hash_sha256_update(&state2, hr, 32);

            h = (cur % 2 == 0) ? hl : hr;
            if(memcmp(h, tmp, 32) != 0) {
                hash_to_string(hash_string, h);
                hash_to_string(hash_string2, tmp);
                printf("Fail, lhs: %s, rhs: %s\nchal: %d, level: %d\n", hash_string, hash_string2, i, j);
                exit(1);
            }
            cur /= 2;
        }
    }

    crypto_hash_sha256_final(&state1, actual_com1);
    crypto_hash_sha256_final(&state2, actual_com2);

    if (memcmp(expected_com1, actual_com1, 32) != 0 || 
        memcmp(expected_com2, actual_com2, 32) != 0) {
            fprintf(stdout, "Failing\n");
            exit(0);
    }
    printf("Verification succeeds\n");
    fclose(proof_file);
}
