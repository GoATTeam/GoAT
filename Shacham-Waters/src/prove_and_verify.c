#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "audit.h"
#include "common.h"
#include "utils.h"

#include <sodium.h>

struct conf {
    char* commitment_file;
    char* out_file;
    int benchmarking;
};

static void print_help(FILE *f, char *p) {
	fprintf(f,"Usage: %s [-b (for benchmark)]\n", p);
}

static void parse_conf(struct conf *cnf, int argc, char *argv[]) {
	int opt; // hb
	while ((opt = getopt(argc, argv, "hb")) != -1) {
		switch (opt) {
            case 'b':
            cnf->benchmarking = true;
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

int main(int argc, char** argv) {
    struct conf cnf = {
        .commitment_file = DEFAULT_P1_OUT,
        .out_file = DEFAULT_P2_OUT,
        .benchmarking = false
    };

    parse_conf(&cnf, argc, argv);

    int n_runs = cnf.benchmarking ? 50 : 1;

    FILE* f = fopen(cnf.commitment_file, "r");
    // Check if file exists
    if (f == NULL)
    {
        printf("Could not open file %s\n", cnf.commitment_file);
        return 0;
    }

    char c;  // To store a character read from file
    int count = 0;
    // Extract characters from file and store in character c
    for (c = getc(f); c != EOF; c = getc(f))
        if (c == '\n') // Increment count if this character is newline
            count = count + 1;

    printf("The file %s has %d lines\n", cnf.commitment_file, count);

    // Start from begin
    fseek(f, 0, SEEK_SET);    
    int n_anchor_ping = count / 2;
    int n_por = n_anchor_ping - 1;
    int n_chal = NUM_CHAL;
    printf("(n_anchor_ping:%d, n_por:%d)\n", n_anchor_ping, n_por);

    char buf64[65];
    buf64[64] = '\0';
    unsigned char por_comm[n_anchor_ping][32];
    unsigned char anchor_sign[n_anchor_ping][32];
    for (int i = 0; i < n_anchor_ping; i++) {
        fread(buf64, 1, 64, f);
        // printf("%s\n", buf64);
        hex64_to_bytes(buf64, por_comm[i]);
        fread(buf64, 1, 1, f);

        fread(buf64, 1, 64, f);
        // printf("%s\n", buf64);
        hex64_to_bytes(buf64, anchor_sign[i]);
        fread(buf64, 1, 1, f);
    }
    fclose(f);

    struct public_key_t* pk = import_pk(DEFAULT_PK_FILE, NUM_SECTORS);
    if (! pk) {
        printf("Error\n");
        exit(0);
    }
    printf("Public key read successfully!\n");

    struct pairing_s *pairing = (struct pairing_s*)malloc(sizeof(pairing_t));
    pbc_pairing_init(pairing);

    struct vc_param_s* params = init_vc_param(pairing);

    double t_prove[n_runs];
    double t_verify[n_runs];

    for (int id = 0; id < n_runs; id++) {
        printf("===============\nRound id: %d\n", id);

        struct timespec t_prove_start, t_prove_end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_prove_start);

        // Gen random indices
        struct element_s* r = (struct element_s*) 
            malloc(sizeof(struct element_s) * n_por);
        unsigned int seed1;
        randombytes_buf_deterministic(&seed1, sizeof (unsigned int), 
                                    por_comm[n_anchor_ping - 1]);
        pbc_random_set_deterministic(seed1);
        for (int i = 0; i < n_por; i++) {
            element_init_Zr(r + i, pairing);
            element_random(r + i);
        }

        struct query_t *Q[n_por];
        struct query_t* aggregate_query = init_query_t(
            n_chal * n_por, 
            pairing
        );
        int index = 0;
        for (int i = 0; i < n_por; i++) {
            Q[i] = init_query_t(n_chal, pairing);
            generate_query_deterministic(Q[i], NUM_BLOCKS, anchor_sign[i]);
            for (int j = 0; j < Q[i]->query_length; j++, index++) {
                aggregate_query->indices[index] = Q[i]->indices[j];
                element_mul(aggregate_query->v + index, Q[i]->v + j, r + i);
            }
        }

        struct file_block_t** blocks = read_select_file_blocks(
            DEFAULT_DATA_FILE,
            aggregate_query->indices,
            aggregate_query->query_length,
            NUM_SECTORS, SS
        );
        if (! cnf.benchmarking)
            printf("File read successfully!\n");

        import_select_tags(DEFAULT_TAG_FILE, blocks, 
                        aggregate_query->query_length);
        // import_tag(DEFAULT_TAG_FILE, tag_info);
        if (! cnf.benchmarking)
            printf("Tags read successfully!\n");

        struct query_response_t* R = query_with_selected_blocks(
            aggregate_query, 
            blocks, 
            pairing
        );

        int ps = element_length_in_bytes_compressed(R->sigma) + 
                 element_length_in_bytes(R->mu_vec) * R->mu_vec_length;
        // struct element_s* mu_1 = R->mu_vec + 1;
        printf("proof_size:%d bytes (muvec_len:%d)\n", ps, R->mu_vec_length);

        // element_printf("sigma: %B\n", R->sigma);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_prove_end);
        t_prove[id] = timediff(&t_prove_start, &t_prove_end);

        if (! cnf.benchmarking) {
            printf("Proving: %fms\n\nVerification begins\n", t_prove[id]);
        }

        struct timespec t_verify_0, t_verify_1, t_verify_2;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_verify_0);

        // Verification #1
        if (verify(aggregate_query, R, pk)) {
            printf("Success #1\n");
        } else {
            printf("FAIL #1\n");
            exit(0);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_verify_1);

        // Verification #2
        element_t C_lhs;
        element_init_G1(C_lhs, pairing);
        element_set1(C_lhs);
        vector_commit_precomputed(R->mu_vec, C_lhs, params);

        // por_comm[1,...,n_anchor_ping-1]: A total of n_por
        struct element_s* commitments = malloc(sizeof(struct element_s) * n_por);
        // element_t commitments[n_por];
        for (int i = 0; i < n_por; i++) {
            int j = i+1;
            element_init_G1(commitments + i, pairing);
            element_from_bytes_compressed(commitments + i, por_comm[j]);
        }
        if (! cnf.benchmarking)
            printf("commitments imported\n");

        element_t C_rhs;
        element_init_G1(C_rhs, pairing);
        element_set1(C_rhs);
        vector_commit_3x(commitments, r, C_rhs, n_por);

        if (element_cmp(C_lhs, C_rhs) == 0) {
            printf("Success #2\n");
        } else {
            printf("Fail #2\n");
            exit(0);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_verify_2);

        t_verify[id] = timediff(&t_verify_0, &t_verify_2);
        if (!cnf.benchmarking)
            printf("Verification: %fms (%f, %f)\n", 
                timediff(&t_verify_0, &t_verify_2),
                timediff(&t_verify_0, &t_verify_1),
                timediff(&t_verify_1, &t_verify_2));

        // End
        free(r);
        free_blocks(blocks, aggregate_query->query_length, NUM_SECTORS);
        free_query_response_t(R);
        free_query_t(aggregate_query);
        for (int i = 0; i < n_por; i++)
            free_query_t(Q[i]);
    }

    printf("\n===========\n");

    printf("Means: prove:%fms verify:%fms\n", 
            mean(t_prove, n_runs), 
            mean(t_verify, n_runs));

    printf("Standard deviation: prove:%fms verify:%fms\n", 
            standard_deviation(t_prove, n_runs), 
            standard_deviation(t_verify, n_runs));

    free(pairing);
}