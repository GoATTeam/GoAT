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

void import_bases(char* filename, g1_t* bases, int size) {
    FILE* fd = fopen(filename, "r");
    if (!fd) {
        printf("%s does not exist, exiting\n", filename);
        exit(1);
    }

    unsigned char data[1000];

    for (int i = 0; i < size; i++) {
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        g1_new(bases[i]);
        g1_read_bin(bases[i], data, G1_LEN_COMPRESSED);
    }
    printf("Import bases finish\n");
    fclose(fd);
}

void vc_setup(char* filename, int size) {
    if (access(filename, F_OK) == 0 ) {
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

    export_bases(filename, bases, size);

    for (int i=0; i<size; i++) {
        g1_free(bases[i]);
    }
}

struct vc_param_s* init_vc_param() {
    struct vc_param_s* params = (struct vc_param_s*) malloc(sizeof(struct vc_param_s));
    params->size = NUM_SECTORS;
    params->bases = (g1_t*) malloc(sizeof(g1_t) * params->size);

    import_bases(DEFAULT_BASES_FILE, params->bases, params->size);

    params->precomp = (g1_t**) malloc(sizeof(g1_t*) * params->size);
    for (int i = 0; i < params->size; i++) {
	    params->precomp[i] = (g1_t*) malloc(sizeof(g1_t) * RLC_G1_TABLE);
    }
    for (int i = 0; i < params->size; i++) {
	    for (int j = 0; j < RLC_G1_TABLE; j++) {
		g1_new(params->precomp[i][j]);	    
	    }
	    g1_mul_pre(params->precomp[i], params->bases[i]);
    }

    return params;
}

void free_vc_param(struct vc_param_s* params) {
	for (int i = 0; i < params->size; i++)
		free(params->precomp[i]);
	free(params->precomp);
    	free(params->bases);
	free(params);
}

void vector_commit_precomputed(
    bn_t* vals, 
    g1_t output,
    struct vc_param_s* params
) {
    g1_t temp;
    g1_new(temp);
    for (int j = 0; j < params->size; j++) { // com = base_1^val_1 x base_2^val_2 x ...
        g1_mul_fix(temp, params->precomp[j], vals[j]);
        g1_add(output, output, temp);
    }
}

//void vector_commit_3x(
//    struct g1_t* bases,
//    struct g1_t* vals,
//    struct g1_t* output,
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
//    struct g1_t** bases,
//    struct g1_t* vals,
//    struct g1_t* output,
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
