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

    element_t hash;
    element_init_G1(hash, params->pairing);
    bls_hash_int(block_index, hash, params->pairing); // hash = H(i)

    struct file_sector_t *sector1, *sector2, *sector3;
    mpz_t f_i1, f_i2, f_i3;
    struct element_s* tag = block->sigma;
    element_init_G1(tag, params->pairing);
    element_set(tag, hash);

    if (block->num_sectors != params->u_size) {
        printf("Critical error in gen_tag\n");
        exit(0);
    }

    element_t acc; // used to accumulate
    element_init_G1(acc, params->pairing);
    
    if (block->num_sectors % 3 != 0) {
        printf("Unimplemented\n");
        exit(0);
    }
    char *hex = malloc(sizeof(char)*((2*SS)+1));
    for (int i = 0; i < block->num_sectors; i+=3) {
        sector1 = block->sectors + i;
        bytes_to_hex(hex, sector1->data, sector1->sector_size);
        mpz_init_set_str(f_i1, hex, 16);

        sector2 = block->sectors + i + 1;
        bytes_to_hex(hex, sector2->data, sector2->sector_size);
        mpz_init_set_str(f_i2, hex, 16);

        sector3 = block->sectors + i + 2;
        bytes_to_hex(hex, sector3->data, sector3->sector_size);
        mpz_init_set_str(f_i3, hex, 16);

        element_pow3_mpz(acc, params->u_vec+i, f_i1, params->u_vec+i+1, f_i2, params->u_vec+i+2, f_i3); // acc = u_i^f_i
        element_mul(tag, acc, tag); // tag = tag * u_i^f_i
    }
    free(hex);

    element_pow_zn(tag, tag, sk->alpha);
}

struct private_key_t *sk;
struct file_t* file_info;

#define num_threads 20
#define load (int) ceil (NUM_BLOCKS / (1.0 * num_threads))

void *runner(void *t)
{
    int i;
    long my_id = (long)t;

    printf("thread %ld. About to start work...\n", my_id);
    int start = my_id * load;
    int end = (my_id + 1) * load; // NOTE: Assumes N is a multiple of load
    if (end > NUM_BLOCKS) {
        end = NUM_BLOCKS;
    }
    printf("start:%d end:%d\n", start, end);
    for (i = start; i < end; i++) {
        gen_tag(i, file_info->blocks+i, sk);
    }

    pthread_exit(NULL);
}

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
    // pthread_attr_t attr;
    // pthread_t threads[num_threads];

    // pthread_attr_init(&attr);
    // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // int i;
    // for (i = 0; i < num_threads; i++) {
    //     long j = i;
    //     pthread_create(&threads[i], &attr, runner, (void *)j);
    // }

    // for (i = 0; i < num_threads; i++) {
    //     pthread_join(threads[i], NULL);
    // }

    // Serial version
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // printf("%d %d\n", i, NUM_BLOCKS);
        gen_tag(i, file_info->blocks+i, sk);
    }

    export_tag(DEFAULT_TAG_FILE, file_info);
    printf("tag export done\n");

    vc_setup(NUM_SECTORS);
    printf("vector commitment bases export done\n");

    free_file_t(file_info, NUM_BLOCKS, NUM_SECTORS);
}