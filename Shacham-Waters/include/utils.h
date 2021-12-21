#pragma once

#include <pbc/pbc.h>

#define DEFAULT_PARAM_FILE "./param/f.param"

/*
Copied from pbc

Initializes pairing from file specified as first argument, or from standard
input if there is no first argument.
*/
static inline void pbc_pairing_init(pairing_t pairing) {
    char s[16384];
    FILE *fp;

    char* filename = DEFAULT_PARAM_FILE;
    // printf("Initializing with %s\n", filename);
    fp = fopen(filename, "r");
    if (!fp) pbc_die("error opening %s", filename);

    size_t count = fread(s, 1, 16384, fp);
    if (!count) pbc_die("input error");
    fclose(fp);

    if (pairing_init_set_buf(pairing, s, count)) pbc_die("pairing init failed");
}

/*
Computes t1-t0 in milliseconds
*/
static inline double timediff(struct timespec* t0, struct timespec* t1) {
    return ((t1->tv_sec - t0->tv_sec) * 1000 + 
                (double) (t1->tv_nsec - t0->tv_nsec) / 1000000);
}

struct common_t {
    struct pairing_s* pairing;
    element_t g;
    struct element_s *u_vec;
    uint32_t u_size;
};

struct public_key_t {
    struct common_t *params;
    element_t v;  // v = g^alpha
};

struct private_key_t {
    struct common_t *params;
    element_t alpha;
};

struct keypair_t {
    struct public_key_t* pub_key;
    struct private_key_t* pvt_key;
};

struct resp {
    char* data;
};

struct keypair_t* generate_key_pair(int public_params_size);

void bls_hash_int(uint32_t, struct element_s*, pairing_t);

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
