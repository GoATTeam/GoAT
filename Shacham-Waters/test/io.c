#include "file.h"
#include "common.h"
#include "utils.h"

#include <unistd.h>
#include <assert.h>

#define TEST_DATA_FILE "testfile.dat"
#define TEST_PK_FILE "testpk.dat"

void file() {
    printf("File IO tests\n");
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
	printf("VC param gen + IO test\n");
	vc_setup(NUM_SECTORS);
	for (int i = 0; i < 1; i++) {
		struct vc_param_s* params = init_vc_param();
		free_vc_param(params);
	}
	printf("VC param tests successful\n");
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
	file();
	// vcparams();
	keys();
}
