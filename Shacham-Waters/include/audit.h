#pragma once

#include <stdint.h>

#include <pbc/pbc.h>
#include <sodium.h>

#include "common.h"
#include "file.h"
#include "utils.h"
#include "vector_commit.h"

struct query_t {
    int query_length;
    uint32_t *indices;
    struct element_s *v;
};

struct query_response_t {
    int mu_vec_length;
    struct element_s *mu_vec;
    element_t sigma;
};

struct response_comm_t {
    int mu_vec_length;
    struct element_s *mu_vec;
    element_t vector_com;
};

// Init
struct query_t* init_query_t(
    int num_challenges, 
    struct pairing_s* pairing
);
struct response_comm_t* init_response_comm_t(
    int num_sectors,
    struct pairing_s* pairing
);

// Free
void free_query_t(struct query_t* Q);
void free_query_response_t(struct query_response_t* R);
void free_response_comm_t(struct response_comm_t* com);

// Generate random query
void generate_query_random(
    struct query_t* query,
    int num_blocks
);
void generate_query_deterministic(
    struct query_t* query,
    int num_blocks,
    const unsigned char seed[randombytes_SEEDBYTES]
);

// SWPoRet.Prove
struct query_response_t* query_with_selected_blocks(
    struct query_t* query, 
    struct file_block_t** selected, 
    struct pairing_s* pairing
);
struct query_response_t* query_with_full_file(
    struct query_t* query, 
    struct file_t* file_info, 
    struct pairing_s* pairing
);

// SWPoRet.verify
int verify(struct query_t* query,
           struct query_response_t* response,
           struct public_key_t* pk);

// SWPoRet.commit (parallel)
void threaded_swcommit_init(
    int rounds, 
    struct query_t* q,
    struct resp *r,
    struct response_comm_t* resp_com,
    struct vc_param_s* params
);
void threaded_swcommit_run();
void threaded_swcommit_finish();
void threaded_swcommit_close();

// SWPoRet.commit (serial)
struct response_comm_t* swcommit_serial(
    struct query_t* query,
    struct resp *responses,
    int num_sectors,
    int sector_size,
    struct response_comm_t* C,
    struct vc_param_s* params,
    int output
);
