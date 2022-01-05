#pragma once

#include "relic.h"
#include "common.h"

static inline void relic_pairing_init() {
        if (core_init() != RLC_OK) {
                core_clean();
        }
        conf_print();

        if (pc_param_set_any() != RLC_OK) {
                // RLC_THROW(ERR_NO_CURVE);
                printf("Exiting\n");
                core_clean();
        }

	// Do some checks
	g1_t p;
	g1_new(p);
	int len = g1_size_bin(p, 1);
	if(len != G1_LEN_COMPRESSED) {
		printf("set g1_len to %d", len);
	}
	g1_free(p);
	
	g2_t q;
	g2_new(q);
	len = g2_size_bin(q, 1);
	if(len != G2_LEN_COMPRESSED) {
		printf("set g2_len to %d", len);
	}
	g2_free(q);
}

/*
Computes t1-t0 in milliseconds
*/
static inline double timediff(struct timespec* t0, struct timespec* t1) {
    return ((t1->tv_sec - t0->tv_sec) * 1000 + 
                (double) (t1->tv_nsec - t0->tv_nsec) / 1000000);
}

struct common_t {
    g2_t g;
    g1_t *u_vec;
    uint32_t u_size;
};

struct public_key_t {
    struct common_t *params;
    g2_t v;  // v = g^alpha
};

struct private_key_t {
    struct common_t *params;
    bn_t alpha;
};

struct keypair_t {
    struct public_key_t* pub_key;
    struct private_key_t* pvt_key;
};

struct resp {
    char* data;
};

struct keypair_t* generate_key_pair(int public_params_size);

void bls_hash_int(uint32_t, ec_t*);

// Bytes <> Hex
char* hexstring(unsigned char* bytes, int len); // malloc's
void bytes_to_hex(char* out, uint8_t* in, int in_len); // assumes already allocated
void hex64_to_bytes(char in[64], char val[32]);

// Import and export of keys
void export_sk(const char* filename, struct private_key_t* sk);
// struct private_key_t* import_sk(const char* filename);
void export_pk(const char* filename, struct public_key_t* pk);
struct public_key_t* import_pk(const char* filename, int u_vec_size);

long mod(long a, long b);

unsigned char* roughtime_get_time(unsigned char* nonce32, char* anchor_name);

double mean(double data[], int);
double standard_deviation(double data[], int N);
