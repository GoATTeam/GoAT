#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "common.h"

void bn_rand_util(bn_t* p) {
    bn_t n;
    bn_new(n);
    g1_get_ord(n);
    bn_rand_mod(*p, n);
    bn_free(n);
}

struct keypair_t* generate_key_pair(int public_params_size)
{
    printf("generate_key_pair begin\n");

    struct keypair_t* kpair = (struct keypair_t*)malloc(sizeof(struct keypair_t));

    struct public_key_t* pub_key = (struct public_key_t*)malloc(sizeof(struct public_key_t));

    struct private_key_t* pv_key = (struct private_key_t*)malloc(sizeof(struct private_key_t));

    relic_pairing_init();
    printf("Relic Initialized successfully\n");

    struct common_t *common_params = (struct common_t*) malloc(sizeof(struct common_t));
    common_params->u_vec = (g1_t*) malloc (sizeof (g1_t) * public_params_size);
    common_params->u_size = public_params_size;

    // Set u_i to random
    for (size_t i = 0; i < public_params_size; i++) {
        g1_new(common_params->u_vec[i]);
        g1_rand(common_params->u_vec[i]);
    }

    // Set g to random
    g2_new(common_params->g);
    g2_rand(common_params->g);

    // Set private key to random
    bn_new(pv_key->alpha);
    bn_rand_util(& pv_key->alpha);
    
    g2_new(pub_key->v) 
    g2_mul(pub_key->v, common_params->g, pv_key->alpha);

    pub_key->params = common_params;
    pv_key->params = common_params;

    kpair->pub_key = pub_key;
    kpair->pvt_key = pv_key;

    printf("generate_key_pair end\n");

    return kpair;
}

void export_sk(const char* filename, struct private_key_t* sk) {
    FILE* fd = fopen(filename, "w");
    unsigned char data[1000];
    int len = bn_size_bin(sk->alpha);
    bn_write_bin(data, len, sk->alpha);
    fwrite(data, 1, len, fd);
    fclose(fd);
}

struct private_key_t* import_sk(const char* filename) {
    // TODO
}

void export_pk(const char* filename, struct public_key_t* pk) {
    FILE* fd = fopen(filename, "w");
    unsigned char data[1000];

    g2_write_bin(data, G2_LEN_COMPRESSED, pk->v, 1);
    fwrite(data, 1, G2_LEN_COMPRESSED, fd);

    g2_write_bin(data, G2_LEN_COMPRESSED, pk->params->g, 1);
    fwrite(data, 1, G2_LEN_COMPRESSED, fd);

    for (int i = 0; i < pk->params->u_size; i++) {
        g1_write_bin(data, G1_LEN_COMPRESSED, pk->params->u_vec[i], 1);
        fwrite(data, 1, G1_LEN_COMPRESSED, fd);
    }

    fclose(fd);
}

struct public_key_t* import_pk(const char* filename, int u_vec_size) {
    struct public_key_t* pk = (struct public_key_t*)malloc(sizeof(struct public_key_t));
    struct common_t *common_params = (struct common_t*) malloc(sizeof(struct common_t));
    common_params->u_vec = (g1_t*) malloc (sizeof (g1_t) * u_vec_size);
    common_params->u_size = u_vec_size;
    pk->params = common_params;

    unsigned char data[1000];

    g2_new(pk->v);
    g2_new(pk->params->g);

    FILE* fd = fopen(filename, "r");
    fread(data, 1, G2_LEN_COMPRESSED, fd);
    g2_read_bin(pk->v, data, G2_LEN_COMPRESSED);

    fread(data, 1, G2_LEN_COMPRESSED, fd);
    g2_read_bin(pk->params->g, data, G2_LEN_COMPRESSED);

    for (int i = 0; i < pk->params->u_size; i++) {
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        g1_new(pk->params->u_vec[i]);
        g1_read_bin(pk->params->u_vec[i], data, G1_LEN_COMPRESSED);
    }
    fclose(fd);
    return pk;
}

// TODO
void free_public_key(struct public_key_t* pk) {

}

void free_private_key(struct private_key_t* pk) {

}

///*
//* 11 because 2^33 has 10 digits.
//* 1 extra for the null termination.
//*/
//char int_str[11]; 
//void bls_hash_int(
//    uint32_t value, 
//    struct element_s* output,
//) {
//    int len = sprintf(int_str, "%u", value);
//    element_from_hash(output, (void*)int_str, len);
//}
//

char* hexstring(unsigned char* bytes, int len)
{
    char* buffer    = malloc(sizeof(char)*((2*len)+1));
    char* temp      = buffer;
    for(int i=0;i<len;i++) {
        sprintf(temp,"%02x",bytes[i]);
        temp += 2;
    }
    *temp = '\0';
    return buffer;
}

void bytes_to_hex(char* out, uint8_t* in, int in_len)
{
	size_t i;
	for (i = 0; i < in_len; i++) {
		out += sprintf(out, "%02x", in[i]);
	}
}

void hex64_to_bytes(char in[64], char val[32]) {
    const char *pos = in;

    for (size_t count = 0; count < 32; count++) {
        sscanf(pos, "%2hhx", &val[count]);
        pos += 2;
    }
}

long mod(long a, long b)
{
    long r = a % b;
    return r < 0 ? r + b : r;
}


#include <unistd.h>
#include <sys/wait.h>

// To run `ls -hl`, set argv to {"ls", "-hl", NULL}
int run(int ARGC, char* argv[]) {
    if (argv[ARGC]) {
        printf("Last argument not set to NULL\n");
        printf("Argument: %s\n", argv[ARGC]);
        exit(0);
        // throw std::invalid_argument("Last argument not set to NULL");
    }

    int pid, status;
    if ((pid = fork())) {
        waitpid(pid, &status, 0);
    } else {
        execvp(argv[0], argv);
    }
    return status;
}

unsigned char* roughtime_get_time(unsigned char* nonce32, char* anchor_name) {
    unsigned char nonce64[64];
    memset(nonce64, 0, 64);
    memcpy(nonce64, nonce32, 32);

    unsigned char hex[128 + 1];
    bytes_to_hex(hex, nonce64, 64);
    hex[128] = '\0';
    printf("[Roughtime] Deriving from %s\n", hex);
    char* tmp_file_for_sig = "rt_sig.tmp";

    {
        char* argv[] = {"rm", tmp_file_for_sig, NULL};
        run(2, argv);
    }

    char transcript_file[50];
    int length = sprintf(transcript_file, "proofs/TRANSCRIPT_RT_");
    for (int i=0; i<4; i++)
        length += sprintf(transcript_file + length, "%02X", nonce64[i]);
    length += sprintf(transcript_file + length, ".out");
    transcript_file[length] = '\0';

    char roughtime_bin[PATH_MAX + 1];
    char *cwd = getcwd(roughtime_bin, PATH_MAX);
    strcat(roughtime_bin, "/../deps/roughenough/target/release/roughenough-client");
    if( access( roughtime_bin, X_OK ) != 0 ) {
        // file doesn't exist
        printf("%s does not exist\n", roughtime_bin);
        exit(0);
    }

    char* argv[] = {roughtime_bin, anchor_name, "2002", 
                    "-t", transcript_file,
                    "-c", hex,
                    NULL};
    run(7, argv);

    FILE* f = fopen(tmp_file_for_sig, "r");
    if (! f) {
        printf("Problem opening file\n");
        exit(0);
    }
    unsigned char* sig = malloc(64);
    fread(sig, 1, 64, f);
    fclose(f);

    bytes_to_hex(hex, sig, 64);
    printf("[Roughtime] Received %s\n", hex);
    return sig;
}

#include <math.h>

double mean(double data[], int N) {
    double sum = 0.0, mean;
    int i;
    for (i = 0; i < N; ++i) {
        sum += data[i];
    }
    return sum / (double) N;
}

double standard_deviation(double data[], int N) {
    double sum = 0.0, mean, SD = 0.0;
    int i;
    for (i = 0; i < N; ++i) {
        sum += data[i];
    }
    mean = sum / (double) N;
    for (i = 0; i < N; ++i) {
        SD += pow(data[i] - mean, 2);
    }
    return sqrt(SD / N);
}
