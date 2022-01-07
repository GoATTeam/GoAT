#include "file.h"
#include "common.h"
#include "utils.h"

#include <unistd.h>
#include <assert.h>

#define TEST_DATA_FILE "testfile.dat"
#define TEST_PK_FILE "testpk.dat"
#define TEST_BASES_FILE "testbases.dat"

void file() {
    printf("===================\nFile IO tests\n");
    printf("Creating a file of size: %fMB\n", FS/ (1024.0 * 1024));

    struct file_t* file_info = create_file(TEST_DATA_FILE, NUM_BLOCKS, NUM_SECTORS, SS);

    {
	struct file_t* tmp = read_file(TEST_DATA_FILE, NUM_BLOCKS, NUM_SECTORS, SS);
    }
    printf("File IO tests successful\n");
}

// void tags() {}

#include "vector_commit.h"

void vcparams() {
	printf("===================\nVC param gen + IO test\n");
	vc_setup(TEST_BASES_FILE, NUM_SECTORS);
	for (int i = 0; i < 2; i++) {
		struct vc_param_s* params = init_vc_param();
		free_vc_param(params);
	}
	printf("VC param IO successful\n");

	struct vc_param_s* params = init_vc_param();
	int len = params->size;
	bn_t vals[len];
	for (int i = 0; i < len; i++) {
		bn_new(vals[i]);
		bn_rand_mod(vals[i], N);
	}
	g1_t out1, out2;
	g1_new(out1);
	g1_set_infty(out1);
	vector_commit_naive(params->bases, vals, out1, len);

	g1_new(out2);
	g1_set_infty(out2);
	vector_commit_precomputed(vals, out2, params);

	if (g1_cmp(out1, out2) != RLC_EQ) {
		printf("VC naive != precomputed. Failure!\n");
		exit(-1);
	}

        char bytes[G1_LEN_COMPRESSED];
        g1_write_bin(bytes, G1_LEN_COMPRESSED, out1, 1);

        g1_t s;
        g1_new(s);
        g1_read_bin(s, bytes, G1_LEN_COMPRESSED);

	if (g1_cmp(out1, s) != RLC_EQ) {
		printf("Weird error\n");
		exit(0);
	}

	printf("VC precompute test successful!\n");
	g1_free(out1);
	g1_free(out2);
	for (int i = 0; i < len; i++) {
		bn_free(vals[i]);
	}
	free_vc_param(params);
}

void keys() {
	struct keypair_t *kpair = generate_key_pair(NUM_SECTORS);
	struct public_key_t *pk = kpair->pub_key;
	struct private_key_t *sk = kpair->pvt_key;

	export_pk(TEST_PK_FILE, pk);

	struct public_key_t *pk_imp = import_pk(TEST_PK_FILE, NUM_SECTORS);

	assert(g2_cmp(pk->v, pk_imp->v) == RLC_EQ);
	assert(g2_cmp(pk->params->g, pk_imp->params->g) == RLC_EQ);
	for (int i = 0; i < NUM_SECTORS; i++) {
		assert(g1_cmp(pk->params->u_vec[i], pk_imp->params->u_vec[i]) == RLC_EQ);
	}
	printf("Keys IO test succesful\n");
}

int main() {
	relic_pairing_init();
	file();
	keys();
	vcparams();
}
