#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "common.h"
#include "file.h"
#include "utils.h"
#include "vector_commit.h"

void gen_tag(int block_index, 
             struct file_block_t* block,
             struct private_key_t *sk) 
{
    struct common_t* params = sk->params;

    uint8_t bytes[1] = {block_index};

    g1_map(block->sigma, bytes, 1); // hash = H(i)

    struct file_sector_t *sector1, *sector2;
    bn_t f_i1, f_i2;
    bn_new(f_i1);
    bn_new(f_i2);

    if (block->num_sectors != params->u_size) {
        printf("Critical error in gen_tag\n");
        exit(0);
    }

    g1_t acc; // used to accumulate
    g1_new(acc);

    if (block->num_sectors % 2 != 0) {
        printf("Unimplemented\n");
        exit(0);
    }
    for (int i = 0; i < block->num_sectors; i+=2) {
	// printf("Hola sectorid:%d\n", i);
	sector1 = block->sectors + i;
        bn_read_bin(f_i1, sector1->data, sector1->sector_size);

        sector2 = block->sectors + i + 1;
        bn_read_bin(f_i2, sector2->data, sector2->sector_size);

	g1_mul_sim(acc, params->u_vec[i], f_i1, params->u_vec[i+1], f_i2);
	g1_add(block->sigma, acc, block->sigma);
    }
    g1_mul(block->sigma, block->sigma, sk->alpha);
}

struct private_key_t *sk;
struct file_t* file_info;

int main() {
    if (access(DEFAULT_DATA_FILE, F_OK) == 0 ) {
        printf("Do you want to overwrite previously generated data? (y/n) ");
        char inp;
        scanf("%c", &inp);
        if (inp != 'y') {
            printf("Terminating\n");
            exit(0);
        }
    }

    printf("Creating a file of size: %fMB\n", FS/ (1024.0 * 1024));

    file_info = create_file(DEFAULT_DATA_FILE, NUM_BLOCKS, NUM_SECTORS, SS);

    printf("file export complete\n");

    struct keypair_t *kpair = generate_key_pair(NUM_SECTORS);
    struct public_key_t *pk = kpair->pub_key;
    sk = kpair->pvt_key;

    export_sk(DEFAULT_SK_FILE, sk);
    export_pk(DEFAULT_PK_FILE, pk);

    printf("sk, pk export complete\n");

    // Generate tags now
    printf("Tag computation begins...\n");

    // Serial version
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // printf("%d %d\n", i, NUM_BLOCKS);
        gen_tag(i, file_info->blocks+i, sk);
    }

    export_tag(DEFAULT_TAG_FILE, file_info);
    printf("tag export done\n");

    vc_setup(DEFAULT_BASES_FILE, NUM_SECTORS);
    printf("vector commitment bases export done\n");

    free_file_t(file_info, NUM_BLOCKS, NUM_SECTORS);
}
