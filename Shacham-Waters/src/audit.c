#include "audit.h"
#include "file.h"
#include "utils.h"

#include "sodium.h"

#include <time.h>

struct response_comm_t* init_response_comm_t(
    int num_sectors
){
    struct response_comm_t* resp_com = (struct response_comm_t*) malloc(sizeof(struct response_comm_t));

    int s = num_sectors;
    resp_com->mu_vec = (bn_t*) malloc(sizeof(bn_t) * s);
    resp_com->mu_vec_length = s;

    for (int j = 0; j < s; j++) {
        bn_new(resp_com->mu_vec[j]);
        bn_zero(resp_com->mu_vec[j]);
    }
    g1_new(resp_com->vector_com);
    g1_set_infty(resp_com->vector_com);

    return resp_com;
}

void free_response_comm_t(struct response_comm_t* com) {
    free(com->mu_vec);
    free(com);
}

struct query_t* init_query_t(
    int num_challenges
) {
    struct query_t* query = (struct query_t*) malloc(sizeof(struct query_t));
    query->query_length = num_challenges;
    query->indices = (uint32_t*) malloc(sizeof(uint32_t) * num_challenges);
    query->v = (bn_t*) malloc( sizeof(bn_t) * num_challenges );
    for (int i = 0; i < num_challenges; i++) {
        bn_new(query->v[i]);
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
    }

    for (int i = 0; i < query->query_length; i++) {
        bn_rand_util(& query->v[i]);
    }
}

void generate_query_deterministic(
    struct query_t* query,
    int num_blocks,
    char* seed,
    int size
) {
    if (! query) {
        printf("Uninitialized query_t. Exiting \n");
        exit(0);
    }
    randombytes_buf_deterministic(query->indices, sizeof(uint32_t) * query->query_length, seed);
    for (int i = 0; i < query->query_length; i++) {
        query->indices[i] = mod(query->indices[i], num_blocks);
        //if (i == 99) {
        //    printf("Index:%d Block:%u\n", i, query->indices[i]);
        //}
    }

    rand_init();
    rand_seed(seed, size);
    for (int i = 0; i < query->query_length; i++) {
        bn_rand_util(& query->v[i]);
        //if (i == 99) {
	//	char hex[size*2 + 1];
	//	hex[size*2] = '\0';
	//	bytes_to_hex(hex, seed, size);
	//	printf("seed:%s, index:%d\n", hex, i);
	//	bn_print(query->v[i]);
	//}
    }
}

struct query_response_t* query_with_selected_blocks(
    struct query_t* query, 
    struct file_block_t** selected
) {
    struct query_response_t* response = malloc(sizeof(struct query_response_t));

    struct timespec t0, t1, t2;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    uint32_t i;
    bn_t* v_i;
    {
        g1_t temp;
        g1_new(temp);

        // First compute sigma
        g1_set_infty(response->sigma);
        g1_t* all_tags[query->query_length];
        for (int k = 0; k < query->query_length; k++) {
            all_tags[k] = & selected[k]->sigma;
        }

        vector_commit_naive_2(all_tags, query->v, response->sigma, query->query_length);

        printf("Sigma computed\n");
        g1_free(temp);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    {
        uint32_t s = selected[0]->num_sectors;
        response->mu_vec = (bn_t*) malloc(sizeof(bn_t) * s);
        response->mu_vec_length = s;
        for (int j = 0; j < s; j++) {
            bn_new(response->mu_vec[j]);
            bn_zero(response->mu_vec[j]);
        }
	bn_t mu[s];
	for (int j = 0; j < s; j++) {
		bn_new(mu[j]);
		bn_zero(mu[j]);
	}
        bn_t f_ij;
	bn_new(f_ij);
        struct file_block_t* block;
        struct file_sector_t* sector;
        bn_t temp;
        bn_new(temp);
	for (int k = 0; k < query->query_length; k++) {
            i = query->indices[k];
            block = selected[k];
            v_i = & query->v[k];
            for (int j = 0; j < s; j++) {
                // mu_j += v_i * f_ij
                sector = block->sectors + j;
                bn_read_bin(f_ij, sector->data, sector->sector_size);
		bn_mul(temp, *v_i, f_ij);
		bn_mod(temp, temp, N);
                bn_add(mu[j], mu[j], temp);
		bn_mod(mu[j], mu[j], N);
            }
        }
	for (int j = 0; j < s; j++) {
		bn_copy(response->mu_vec[j], mu[j]);
	}
        printf("Mu computed\n");
	bn_free(f_ij);
	bn_free(temp);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    printf("sigma:%f mu:%f\n", timediff(&t0, &t1), timediff(&t1, &t2));

    return response;
}

struct query_response_t* query_with_full_file(
    struct query_t* query, 
    struct file_t* file_info
) {
    struct file_block_t** selected = 
        (struct file_block_t**) malloc(sizeof(struct file_block_t*) * query->query_length);

    for (int k = 0; k < query->query_length; k++) {
        selected[k] = file_info->blocks + query->indices[k];
    }
    printf("Selection done\n");

    struct query_response_t* ret = query_with_selected_blocks(query, selected);
    free(selected);
    return ret;
}

int verify(struct query_t* query,
           struct query_response_t* response,
           struct public_key_t* pk) 
{
    struct timespec t0, t1, t1_5, t2, t3;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    gt_t lhs, rhs;
    gt_new(lhs);
    gt_new(rhs);


    // Calculate lhs = e(sigma, g)
    pc_map(lhs, response->sigma, pk->params->g);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    // rhs now
    g1_t rhs_l, temp;
    g1_new(rhs_l);
    g1_new(temp);
    g1_set_infty(rhs_l);

    g1_t hashes[query->query_length];
    uint8_t bytes[1] = {0};
    for (int k = 0; k < query->query_length; k++) {
	bytes[0] = query->indices[k];
        g1_new(hashes[k]);
        g1_map(hashes[k], bytes, 1);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_5);

    // rhs_l *= hash(i) ^ v_i
    printf("Query length: %d\n", query->query_length);
    vector_commit_naive(hashes, query->v, rhs_l, query->query_length);

    uint32_t s = response->mu_vec_length;
    vector_commit_naive(pk->params->u_vec, response->mu_vec, rhs_l, s);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    pc_map(rhs, rhs_l, pk->v);
    int ret = (gt_cmp(lhs, rhs) == RLC_EQ);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t3);

    printf("p:%f h:%f exp:%f p:%f\n", timediff(&t0, &t1),
                            timediff(&t1, &t1_5),
                            timediff(&t1_5, &t2),
                            timediff(&t2, &t3));

    gt_free(lhs);
    gt_free(rhs);
    g1_free(temp);
    g1_free(rhs_l);
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
#define num_threads 15
#define load (int) ceil (SIZE / (1.0 * num_threads))

static struct vc_param_s* global_params;

static int n_rounds;
static struct response_comm_t* C;
static struct query_t* Q;
static struct resp *R;

static g1_t IP[SIZE];
static g1_t IS[num_threads];
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
    bn_t* v_i;
    bn_t temp;
    bn_new(temp);
    bn_t f_ij;
    bn_new(f_ij);

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
            v_i = & Q->v[k];
            for (int j = start; j < end; j++) {
                sector = block + (SS * j);
                bn_read_bin(f_ij, sector, SS);
		bn_mul(temp, *v_i, f_ij);
		bn_mod(temp, temp, N);
                bn_add(C->mu_vec[j], C->mu_vec[j], temp);
		bn_mod(C->mu_vec[j], C->mu_vec[j], N);
            }
        }

        for (int i = start; i < end; i++) {
	    g1_mul_fix(IP[i], global_params->precomp[i], C->mu_vec[i]);
        }

        g1_t* com = & IS[my_id];
        for (int i = start; i < end; i++) {
            g1_add(*com, *com, IP[i]);
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
        g1_new(IP[i]);
        bn_new(C->mu_vec[i]);
    }

    for (int i=0; i<num_threads; i++) {
        g1_new(IS[i]);
    }
}

void threaded_swcommit_run() {
    for (int j = 0; j < SIZE; j++) {
	bn_zero(C->mu_vec[j]);
    }

    for (int i = 0; i < num_threads; i++) {
        g1_set_infty(IS[i]);
        finished[i] = 0;
        pthread_mutex_lock(&mutex[i]);
        pthread_cond_signal(&cv[i]);
        pthread_mutex_unlock(&mutex[i]);
    }
}

void threaded_swcommit_finish() {
    g1_set_infty(C->vector_com);
    for (int i = 0; i < num_threads; i++) {
        while (!finished[i]) {
            // printf("Waiting for comp to finish\n");
        }
        g1_add(C->vector_com, C->vector_com, IS[i]);
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
		bn_new(C->mu_vec[j]);
		bn_zero(C->mu_vec[j]);
	}
	bn_t f_ij;
	bn_new(f_ij);
	uint32_t i;
	char* block;
	char* sector;
	bn_t temp;
	bn_new(temp);
	bn_t* v_i;
	for (int k = 0; k < query->query_length; k++) {
	  i = query->indices[k];
	  block = responses[k].data;
	  v_i = & query->v[k];
	  for (int j = 0; j < s; j++) {
	      // mu_j += v_i * f_ij
	      sector = block + sector_size * j;
	      bn_read_bin(f_ij, sector, sector_size);
	      bn_mul(temp, *v_i, f_ij);
	      bn_mod(temp, temp, N);
	      bn_add(C->mu_vec[j], C->mu_vec[j], temp); 
	      bn_mod(C->mu_vec[j], C->mu_vec[j], N);
	  }
	}
	printf("Mu computed\n");
	bn_free(f_ij);
	bn_free(temp);
	if (output) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	}

	g1_set_infty(C->vector_com);
	vector_commit_precomputed(C->mu_vec, C->vector_com, params);

	if (output) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
		printf("linear_comb (%f), vc (%f)\n", timediff(&t0, &t1), timediff(&t1, &t2));
	}

	return C;
}

