#pragma once

#include <relic.h>

// One-time setup to create bases
void vc_setup(int size);

struct vc_param_s {
    g1_t* bases;
    // g1_t* precomp[RLC_G1_TABLE];
    int size;
};

// Initialize & Free
struct vc_param_s* init_vc_param();
void free_vc_param(struct vc_param_s* params);

//void vector_commit_precomputed(
//    struct element_s* vals, 
//    struct element_s* output,
//    struct vc_param_s* params
//);
//
//void vector_commit_3x(
//    struct element_s* bases,
//    struct element_s* vals,
//    struct element_s* output,
//    int size
//);
////
//void vector_commit_3x_2(
//    struct element_s** bases,
//    struct element_s* vals,
//    struct element_s* output,
//    int size
//);
////
void vector_commit_naive(
    g1_t* bases,
    bn_t* vals,
    g1_t output,
    int size
);

void vector_commit_naive_2(
    g1_t** bases,
    bn_t* vals,
    g1_t output,
    int size
);
