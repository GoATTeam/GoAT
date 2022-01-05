#include "common.h"
#include "utils.h"
#include "vector_commit.h"

#include <assert.h>
#include <unistd.h>

void export_bases(char* filename, g1_t *bases, int size) {
    FILE* fd = fopen(filename, "w");

    unsigned char data[1000];
    int len;
    len = g1_size_bin(bases[0], 1);
    assert (len < 1000);
    for (int i = 0; i < size; i++) {
        g1_write_bin(data, len, bases[i], 1);
        fwrite(data, 1, len, fd);
    }
    fclose(fd);
}

void import_bases(char* filename, struct vc_param_s* vcparams) {
    FILE* fd = fopen(filename, "r");
    unsigned char data[1000];

    for (int i = 0; i < vcparams->size; i++) {
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        g1_new(vcparams->bases[i]);
        g1_read_bin(vcparams->bases[i], data, G1_LEN_COMPRESSED);
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

    g1_t bases[size];
    for (int i=0; i<size; i++) {
        g1_new(bases[i]);
        g1_rand(bases[i]);
    }

    export_bases(DEFAULT_BASES_FILE, bases, size);

    for (int i=0; i<size; i++) {
        g1_free(bases[i]);
    }
}

struct vc_param_s* init_vc_param() {
    struct vc_param_s* params = (struct vc_param_s*) malloc(sizeof(struct vc_param_s));
    params->size = NUM_SECTORS;
    // params->precomp = (struct element_pp_s*) malloc(sizeof(struct element_pp_s) * params->size);
    params->bases = (g1_t*) malloc(sizeof(g1_t) * params->size);

    import_bases(DEFAULT_BASES_FILE, params);

    // for (int i=0; i<params->size; i++) {
    //    element_pp_init(params->precomp + i, params->bases + i);
    // }

    return params;
}

void free_vc_param(struct vc_param_s* params) {
    // free(params->precomp);
    free(params->bases);
    free(params);
}

//void vector_commit_precomputed(
//    struct element_s* vals, 
//    struct element_s* output,
//    struct vc_param_s* params
//) {
//    element_t temp;
//    element_init_same_as(temp, output);
//    for (int j = 0; j < params->size; j++) { // com = base_1^val_1 x base_2^val_2 x ...
//        element_pp_pow_zn(temp, vals + j, params->precomp + j);
//        element_mul(output, output, temp);
//    }
//}
//
//void vector_commit_3x(
//    struct element_s* bases,
//    struct element_s* vals,
//    struct element_s* output,
//    int size
//) {
//    element_t temp;
//    element_init_same_as(temp, output);
//
//    int i = 0;
//    for (; i < size / 3; i++) {
//        element_pow3_zn(temp, bases + 3*i, vals + 3*i,
//                              bases + 3*i + 1, vals + 3*i + 1,
//                              bases + 3*i + 2, vals + 3*i + 2);
//        // element_pow_zn(temp, bases + i, vals + i);
//        element_mul(output, output, temp);
//    }
//    i *= 3;
//    for (; i < size; i++) {
//        element_pow_zn(temp, bases + i, vals + i);
//        element_mul(output, output, temp);
//    }
//}
//
//void vector_commit_3x_2(
//    struct element_s** bases,
//    struct element_s* vals,
//    struct element_s* output,
//    int size
//) {
//    element_t temp;
//    element_init_same_as(temp, output);
//
//    int i = 0;
//    for (; i < size / 3; i++) {
//        element_pow3_zn(temp, bases[3*i], vals + 3*i,
//                              bases[3*i + 1], vals + 3*i + 1,
//                              bases[3*i + 2], vals + 3*i + 2);
//        // element_pow_zn(temp, bases + i, vals + i);
//        element_mul(output, output, temp);
//    }
//    i *= 3;
//    for (; i < size; i++) {
//        element_pow_zn(temp, bases[i], vals + i);
//        element_mul(output, output, temp);
//    }
//}
//
void vector_commit_naive(
    g1_t* bases,
    bn_t* vals,
    g1_t output,
    int size
) {
    g1_t temp;
    g1_new(temp);

    for (int i = 0; i < size; i++) {
        g1_mul(temp, bases[i], vals[i]);
        g1_add(output, output, temp);
    }
    g1_free(temp);
}

void vector_commit_naive_2(
    g1_t** bases,
    bn_t* vals,
    g1_t output,
    int size
) {
    g1_t temp;
    g1_new(temp);

    for (int i = 0; i < size; i++) {
        g1_mul(temp, *(bases[i]), vals[i]);
        g1_add(output, output, temp);
    }
    g1_free(temp);
}
