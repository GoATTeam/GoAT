#include "audit.h"
#include "file.h"
#include "utils.h"

#include "sodium.h"

#include <time.h>

struct response_comm_t* init_response_comm_t(
    int num_sectors,
    struct pairing_s* pairing
){
    struct response_comm_t* resp_com = (struct response_comm_t*) malloc(sizeof(struct response_comm_t));

    int s = num_sectors;
    resp_com->mu_vec = (struct element_s*) malloc(sizeof(struct element_s) * s);
    resp_com->mu_vec_length = s;

    for (int j = 0; j < s; j++) {
        element_init_Zr(resp_com->mu_vec + j, pairing);
        element_set0(resp_com->mu_vec + j);
    }
    element_init_G1(resp_com->vector_com, pairing);
    element_set1(resp_com->vector_com);

    return resp_com;
}

void free_response_comm_t(struct response_comm_t* com) {
    free(com->mu_vec);
    free(com);
}

struct query_t* init_query_t(
    int num_challenges, 
    struct pairing_s* pairing
) {
    struct query_t* query = (struct query_t*) malloc(sizeof(struct query_t));
    query->query_length = num_challenges;
    query->indices = (uint32_t*) malloc(sizeof(uint32_t) * num_challenges);
    query->v = (struct element_s*) malloc( sizeof(struct element_s) * num_challenges );
    for (int i = 0; i < num_challenges; i++) {
        element_init_Zr(query->v + i, pairing);
    }
    return query;
}

void free_query_t(struct query_t* query) {
    free(query->indices);
    free(query->v);
    free(query);
}

void generate_query_random(
    struct query_t* query,
    int num_blocks
) {
    if (! query) {
        printf("Uninitialized query_t. Exiting \n");
        exit(0);
    }
    randombytes_buf(query->indices, sizeof(uint32_t) * query->query_length);
    for (int i = 0; i < query->query_length; i++) {
        query->indices[i] = mod(query->indices[i], num_blocks);
        // printf("%d %u\n", i, query->indices[i]);
    }

    unsigned int seed1;
    randombytes_buf(& seed1, sizeof (unsigned int));
    pbc_random_set_deterministic(seed1);
    for (int i = 0; i < query->query_length; i++) {
        element_random(query->v + i);
        // element_printf("v_i: %B\n", query->v + i);
    }
}

void generate_query_deterministic(
    struct query_t* query,
    int num_blocks,
    const unsigned char seed[randombytes_SEEDBYTES]
) {
    if (! query) {
        printf("Uninitialized query_t. Exiting \n");
        exit(0);
    }
    randombytes_buf_deterministic(query->indices, sizeof(uint32_t) * query->query_length, seed);
    for (int i = 0; i < query->query_length; i++) {
        query->indices[i] = mod(query->indices[i], num_blocks);
        // if (i == 23) {
        //     printf("%d %u\n", i, query->indices[i]);
        // }
    }

    unsigned int seed1;
    randombytes_buf_deterministic(&seed1, sizeof (unsigned int), seed);
    pbc_random_set_deterministic(seed1);
    for (int i = 0; i < query->query_length; i++) {
        element_random(query->v + i);
        // if (i == 23) {
        //     element_printf("v_i: %B\n", query->v + i);
        // }
    }
}

struct query_response_t* query_with_selected_blocks(
    struct query_t* query, 
    struct file_block_t** selected, 
    struct pairing_s* pairing
) {
    struct query_response_t* response = malloc(sizeof(struct query_response_t));

    struct timespec t0, t1, t2;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    uint32_t i;
    struct element_s* v_i;
    {
        element_t temp;
        element_init_G1(temp, pairing);

        // First compute sigma
        struct element_s* sigma = response->sigma;
        element_init_G1(sigma, pairing);
        element_set1(sigma);
        struct element_s* all_tags[query->query_length];
        for (int k = 0; k < query->query_length; k++) {
            all_tags[k] = selected[k]->sigma;
        }

        vector_commit_3x_2(all_tags, query->v, sigma, query->query_length);

        // printf("Sigma computed\n");
        element_clear(temp);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    {
        uint32_t s = selected[0]->num_sectors;
        response->mu_vec = (struct element_s*) malloc(sizeof(struct element_s) * s);
        response->mu_vec_length = s;
        for (int j = 0; j < s; j++) {
            element_init_Zr(response->mu_vec + j, pairing);
            element_set0(response->mu_vec + j);
        }
        mpz_t f_ij;
        mpz_init(f_ij);
        struct file_block_t* block;
        struct file_sector_t* sector;
        element_t temp;
        element_init_Zr(temp, pairing);
        for (int k = 0; k < query->query_length; k++) {
            i = query->indices[k];
            block = selected[k];
            v_i = query->v + k;
            for (int j = 0; j < s; j++) {
                // mu_j += v_i * f_ij
                sector = block->sectors + j;
                mpz_import(f_ij, sector->sector_size, 1, 1, 0, 0, sector->data);
                element_mul_mpz(temp, v_i, f_ij);
                element_add(response->mu_vec + j, response->mu_vec + j, temp);
            }
        }
        mpz_clear(f_ij);
        element_clear(temp);
        // printf("Mu computed\n");
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    printf("sigma:%f mu:%f\n", timediff(&t0, &t1), timediff(&t1, &t2));

    return response;
}

struct query_response_t* query_with_full_file(
    struct query_t* query, 
    struct file_t* file_info, 
    struct pairing_s* pairing
) {
    struct file_block_t** selected = 
        (struct file_block_t**) malloc(sizeof(struct file_block_t*) * query->query_length);

    for (int k = 0; k < query->query_length; k++) {
        selected[k] = file_info->blocks + query->indices[k];
    }
    printf("Selection done\n");

    struct query_response_t* ret = query_with_selected_blocks(query, selected, pairing);
    free(selected);
    return ret;
}

int verify(struct query_t* query,
           struct query_response_t* response,
           struct public_key_t* pk) 
{
    struct timespec t0, t1, t1_5, t2, t3;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    struct pairing_s* pairing = pk->params->pairing;

    element_t lhs, rhs;
    element_init_GT(lhs, pairing);
    element_init_GT(rhs, pairing);

    // Calculate lhs = e(sigma, g)
    pairing_apply(lhs, response->sigma, pk->params->g, pairing);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    // rhs now
    uint32_t i;
    struct element_s *v_i;
    element_t rhs_l, temp;
    element_init_G1(rhs_l, pairing);
    element_init_G1(temp, pairing);
    element_set1(rhs_l);

    struct element_s hashes[query->query_length];
    for (int k = 0; k < query->query_length; k++) {
        i = query->indices[k];
        element_init_G1(&hashes[k], pairing);
        bls_hash_int(i, &hashes[k], pairing);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_5);

    // rhs_l *= hash(i) ^ v_i
    // printf("Query length: %d\n", query->query_length);
    vector_commit_3x(hashes, query->v, rhs_l, query->query_length);

    uint32_t s = response->mu_vec_length;
    vector_commit_3x(pk->params->u_vec, response->mu_vec, rhs_l, s);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    pairing_apply(rhs, rhs_l, pk->v, pairing);
    // element_printf("LHS:%B\nRHS:%B\n", lhs, rhs);
    int ret = (element_cmp(lhs, rhs) == 0);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t3);

    printf("p:%f h:%f exp:%f p:%f\n", timediff(&t0, &t1),
                            timediff(&t1, &t1_5),
                            timediff(&t1_5, &t2),
                            timediff(&t2, &t3));

    element_clear(lhs);
    element_clear(rhs);
    element_clear(temp);
    element_clear(rhs_l);
    return ret;
}

void free_query_response_t(struct query_response_t* R) {
    free(R->mu_vec);
    free(R);
}

#include <pthread.h>
#include <math.h>
#include <assert.h>

#include "vector_commit.h"

#define SIZE NUM_SECTORS
#define num_threads 10
#define load (int) ceil (SIZE / (1.0 * num_threads))

static struct vc_param_s* global_params;
static element_t temp_g1;

static int n_rounds;
static struct response_comm_t* C;
static struct query_t* Q;
static struct resp *R;

static struct element_s IP[SIZE];
static struct element_s IS[num_threads];
static int finished[num_threads];

static pthread_mutex_t mutex[num_threads];
static pthread_cond_t cv[num_threads];
static pthread_t threads[num_threads];
static pthread_attr_t attr;

static void *runner(void *t) {
    long my_id = (long)t;
    uint32_t idx;
    char* block;
    char* sector;
    struct element_s* v_i;
    element_t temp_zr;
    element_init_Zr(temp_zr, global_params->pairing);
    mpz_t f_ij;
    mpz_init(f_ij);

    printf("do_commit(): thread %ld. Going into wait...\n", my_id);

    for (int j = 0; j < n_rounds; j++) {
        pthread_cond_wait(&cv[my_id], &mutex[my_id]);
        pthread_mutex_unlock(&mutex[my_id]);

        // printf("thread %ld round %d. About to start work...\n", my_id, j);
        int start = my_id * load;
        int end = (my_id + 1) * load;
        if (end > SIZE) {
            end = SIZE;
        }
        // printf("(%d %d)\n", start, end);
        for (int k = 0; k < Q->query_length; k++) {
            idx = Q->indices[k];
            block = R[k].data;
            v_i = Q->v + k;
            for (int j = start; j < end; j++) {
                sector = block + (SS * j);
                mpz_import(f_ij, SS, 1, 1, 0, 0, sector);
                element_mul_mpz(temp_zr, v_i, f_ij);
                element_add(C->mu_vec + j, C->mu_vec + j, temp_zr);
            }
        }

        for (int i = start; i < end; i++) {
            element_pp_pow_zn(IP + i, C->mu_vec + i, global_params->precomp + i);
        }

        struct element_s *com = IS + my_id;
        for (int i = start; i < end; i++) {
            element_mul(com, com, IP + i);
        }
        finished[my_id] = 1;
    }

    pthread_exit(NULL);
}

/**
 * Creates worker threads that help in SW.commit.
 */
void threaded_swcommit_init(
    int rounds, 
    struct query_t* q,
    struct resp *r,
    struct response_comm_t* resp_com,
    struct vc_param_s* params
) {
    global_params = params;
    if (global_params->size != SIZE) {
        printf("Sizes do not match. Have you called init_vc_param?\n");
        assert(0);
    }

    // Check that SIZE is in between num_threads * (load - 1) and num_threads * load.
    if (num_threads * load < SIZE || num_threads * (load - 1) > SIZE) {
        printf("#threads, load: (%d %d)\n", num_threads, load);
        printf("Not enough load (or) threads\n");
        assert(0);
    }

    Q = q;
    R = r;
    C = resp_com;

    element_init_same_as(temp_g1, params->bases);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    n_rounds = rounds;
    long i;
    for (i = 0; i < num_threads; i++) {
        pthread_mutex_init(&mutex[i], NULL);
        pthread_cond_init(&cv[i], NULL);
        pthread_create(&threads[i], &attr, runner, (void *)i);
    }

    for (int i=0; i<SIZE; i++) {
        element_init_G1(&IP[i], global_params->pairing);
    }

    for (int i=0; i<num_threads; i++) {
        element_init_G1(&IS[i], global_params->pairing);
    }
}

void threaded_swcommit_run() {
    for (int j = 0; j < SIZE; j++) {
        element_set0(C->mu_vec + j);
    }

    for (int i = 0; i < num_threads; i++) {
        element_set0(&IS[i]);
        finished[i] = 0;
        pthread_mutex_lock(&mutex[i]);
        pthread_cond_signal(&cv[i]);
        pthread_mutex_unlock(&mutex[i]);
    }
}

void threaded_swcommit_finish() {
    struct element_s* output = C->vector_com;
    element_set1(output);
    for (int i = 0; i < num_threads; i++) {
        while (!finished[i]) {
            // printf("Waiting for comp to finish\n");
        }
        element_mul(output, output, &IS[i]);
    }
}

void threaded_swcommit_close() {
    /* Clean up and exit */
    pthread_attr_destroy(&attr);
    for (int i = 0; i < num_threads; i++) {
        pthread_mutex_destroy(&mutex[i]);
        pthread_cond_destroy(&cv[i]);
        pthread_join(threads[i], NULL);
    }
}

struct response_comm_t* swcommit_serial(
    struct query_t* query,
    struct resp *responses,
    int num_sectors,
    int sector_size,
    struct response_comm_t* C,
    struct vc_param_s* params,
    int output
) {
    if (output) 
        printf("In commit\n");
    struct timespec t0, t1, t2, t3, t4;
    if (output) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    }
    int s = num_sectors;
    for (int j = 0; j < s; j++) {
        element_set0(C->mu_vec + j);
    }

    mpz_t f_ij;
    element_t temp_zr;
    mpz_init(f_ij);
    element_init_Zr(temp_zr, params->pairing);

    uint32_t i;
    struct element_s* v_i;
    char* block;
    char* sector;
    for (int k = 0; k < query->query_length; k++) {
        // printf("k: %d\n", k);
        i = query->indices[k];
        block = responses[k].data;
        v_i = query->v + k;
        for (int j = 0; j < s; j++) {
            // mu_j += v_i * f_ij
            sector = block + (sector_size * j);
            mpz_import(f_ij, sector_size, 1, 1, 0, 0, sector);
            element_mul_mpz(temp_zr, v_i, f_ij);
            element_add(C->mu_vec + j, C->mu_vec + j, temp_zr);
        }
    }
    if (output) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    }
    // printf("Mu computed\n");

    element_set1(C->vector_com);
    vector_commit_precomputed(C->mu_vec, C->vector_com, params);

    if (output) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
        printf("linear_comb (%f), vc (%f)\n", timediff(&t0, &t1), timediff(&t1, &t2));
    }

    mpz_clear(f_ij);
    element_clear(temp_zr);

    return C;
}