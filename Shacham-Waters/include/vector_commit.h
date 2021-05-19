#pragma once

#include <pbc/pbc.h>

// One-time setup to create bases
void vc_setup(int size);

struct vc_param_s {
    struct element_s* bases;
    struct element_pp_s* precomp;
    int size;
    struct pairing_s* pairing;
};

// Initialize & Free
struct vc_param_s* init_vc_param(struct pairing_s* pairing);
void free_vc_param(struct vc_param_s* params);

void vector_commit_precomputed(
    struct element_s* vals, 
    struct element_s* output,
    struct vc_param_s* params
);

void vector_commit_3x(
    struct element_s* bases,
    struct element_s* vals,
    struct element_s* output,
    int size
);

void vector_commit_3x_2(
    struct element_s** bases,
    struct element_s* vals,
    struct element_s* output,
    int size
);

void vector_commit_naive(
    struct element_s* bases,
    struct element_s* vals,
    struct element_s* output,
    int size
);