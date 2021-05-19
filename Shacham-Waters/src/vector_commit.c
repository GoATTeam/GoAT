#include "common.h"
#include "utils.h"
#include "vector_commit.h"

#include <assert.h>
#include <unistd.h>

void export_bases(char* filename, struct element_s *bases, int size) {
    FILE* fd = fopen(filename, "w");

    unsigned char data[1000];
    int len;
    len = element_length_in_bytes_compressed(bases);
    assert (len < 1000);
    for (int i = 0; i < size; i++) {
        element_to_bytes_compressed(data, bases + i);
        fwrite(data, 1, len, fd);
    }
    fclose(fd);
}

void import_bases(char* filename, struct vc_param_s* vcparams) {
    FILE* fd = fopen(filename, "r");
    unsigned char data[1000];

    int G1_LEN_COMPRESSED = pairing_length_in_bytes_compressed_G1(vcparams->pairing);
    for (int i = 0; i < vcparams->size; i++) {
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        element_init_G1(vcparams->bases + i, vcparams->pairing);
        element_from_bytes_compressed(vcparams->bases + i, data);
    }
    printf("Import bases finish\n");
    fclose(fd);
}

void vc_setup(int size) {
    if (access(DEFAULT_BASES_FILE, F_OK) == 0 ) {
        printf("Do you want to overwrite previously generated bases? (y/n) ");
        char inp;
        scanf("%c", &inp);
        if (inp != 'y') {
            printf("Terminating\n");
            exit(0);
        }
    }

    pairing_t pairing;
    pbc_pairing_init(pairing);
    struct element_s bases[size];
    for (int i=0; i<size; i++) {
        element_init_G1(&bases[i], pairing);
        element_random(&bases[i]);
    }

    export_bases(DEFAULT_BASES_FILE, bases, size);

    for (int i=0; i<size; i++) {
        element_clear(&bases[i]);
    }
}

struct vc_param_s* init_vc_param(struct pairing_s* pairing) {
    struct vc_param_s* params = (struct vc_param_s*) malloc(sizeof(struct vc_param_s));
    params->size = NUM_SECTORS;
    params->precomp = (struct element_pp_s*) malloc(sizeof(struct element_pp_s) * params->size);
    params->bases = (struct element_s*) malloc(sizeof(struct element_s) * params->size);
    params->pairing = pairing;

    import_bases(DEFAULT_BASES_FILE, params);

    for (int i=0; i<params->size; i++) {
        element_pp_init(params->precomp + i, params->bases + i);
    }

    return params;
}

void free_vc_param(struct vc_param_s* params) {
    free(params->precomp);
    free(params->bases);
    free(params);
}

void vector_commit_precomputed(
    struct element_s* vals, 
    struct element_s* output,
    struct vc_param_s* params
) {
    element_t temp;
    element_init_same_as(temp, output);
    for (int j = 0; j < params->size; j++) { // com = base_1^val_1 x base_2^val_2 x ...
        element_pp_pow_zn(temp, vals + j, params->precomp + j);
        element_mul(output, output, temp);
    }
}

void vector_commit_3x(
    struct element_s* bases,
    struct element_s* vals,
    struct element_s* output,
    int size
) {
    element_t temp;
    element_init_same_as(temp, output);

    int i = 0;
    for (; i < size / 3; i++) {
        element_pow3_zn(temp, bases + 3*i, vals + 3*i,
                              bases + 3*i + 1, vals + 3*i + 1,
                              bases + 3*i + 2, vals + 3*i + 2);
        // element_pow_zn(temp, bases + i, vals + i);
        element_mul(output, output, temp);
    }
    i *= 3;
    for (; i < size; i++) {
        element_pow_zn(temp, bases + i, vals + i);
        element_mul(output, output, temp);
    }
}

void vector_commit_3x_2(
    struct element_s** bases,
    struct element_s* vals,
    struct element_s* output,
    int size
) {
    element_t temp;
    element_init_same_as(temp, output);

    int i = 0;
    for (; i < size / 3; i++) {
        element_pow3_zn(temp, bases[3*i], vals + 3*i,
                              bases[3*i + 1], vals + 3*i + 1,
                              bases[3*i + 2], vals + 3*i + 2);
        // element_pow_zn(temp, bases + i, vals + i);
        element_mul(output, output, temp);
    }
    i *= 3;
    for (; i < size; i++) {
        element_pow_zn(temp, bases[i], vals + i);
        element_mul(output, output, temp);
    }
}

void vector_commit_naive(
    struct element_s* bases,
    struct element_s* vals,
    struct element_s* output,
    int size
) {
    element_t temp;
    element_init_same_as(temp, output);

    for (int i = 0; i < size; i++) {
        element_pow_zn(temp, bases + i, vals + i);
        element_mul(output, output, temp);
    }
}