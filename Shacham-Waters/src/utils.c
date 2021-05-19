#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "common.h"

struct keypair_t* generate_key_pair(int public_params_size)
{
    printf("generate_key_pair begin\n");

    struct keypair_t* kpair = (struct keypair_t*)malloc(sizeof(struct keypair_t));

    struct public_key_t* pub_key = (struct public_key_t*)malloc(sizeof(struct public_key_t));

    struct private_key_t* pv_key = (struct private_key_t*)malloc(sizeof(struct private_key_t));

    struct pairing_s *pairing = (struct pairing_s*)malloc(sizeof(pairing_t));
    pbc_pairing_init(pairing);

    struct common_t *common_params = (struct common_t*) malloc(sizeof(struct common_t));
    common_params->u_vec = (struct element_s*) malloc (sizeof (struct element_s) * public_params_size);
    common_params->u_size = public_params_size;
    common_params->pairing = pairing;

    // Set u_i to random
    for (size_t i = 0; i < public_params_size; i++) {
        element_init_G1(common_params->u_vec + i, pairing);
        element_random(common_params->u_vec + i);
    }

    // Set g to random
    if (pairing_is_symmetric(pairing)) {
        element_init_G1(common_params->g, pairing);
    } else {
        element_init_G2(common_params->g, pairing);
    }
    element_random(common_params->g);

    // Set private key to random
    element_init_Zr(pv_key->alpha, pairing);
    element_random(pv_key->alpha);

    if (pairing_is_symmetric(pairing)) {
        element_init_G1(pub_key->v, pairing);
    } else {
        element_init_G2(pub_key->v, pairing);
    }
    element_pow_zn(pub_key->v, common_params->g, pv_key->alpha);

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
    int len = element_to_bytes(data, sk->alpha);
    fwrite(data, 1, len, fd);
    fclose(fd);
}

struct private_key_t* import_sk(const char* filename) {
    // TODO
}

void export_pk(const char* filename, struct public_key_t* pk) {
    FILE* fd = fopen(filename, "w");
    unsigned char data[1000];

    int len;
    len = element_to_bytes_compressed(data, pk->v);
    assert (len < 1000);
    fwrite(data, 1, len, fd);

    len = element_to_bytes_compressed(data, pk->params->g);
    assert (len < 1000);
    fwrite(data, 1, len, fd);

    len = element_length_in_bytes_compressed(pk->params->u_vec);
    assert (len < 1000);
    for (int i = 0; i < pk->params->u_size; i++) {
        element_to_bytes_compressed(data, pk->params->u_vec + i);
        fwrite(data, 1, len, fd);
    }

    fclose(fd);
}

struct public_key_t* import_pk(const char* filename, int u_vec_size) {
    struct pairing_s *pairing = (struct pairing_s*)malloc(sizeof(pairing_t));
    pbc_pairing_init(pairing);

    struct public_key_t* pk = (struct public_key_t*)malloc(sizeof(struct public_key_t));
    struct common_t *common_params = (struct common_t*) malloc(sizeof(struct common_t));
    common_params->u_vec = (struct element_s*) malloc (sizeof (struct element_s) * u_vec_size);
    common_params->u_size = u_vec_size;
    common_params->pairing = pairing;
    pk->params = common_params;

    int G1_LEN_COMPRESSED = pairing_length_in_bytes_compressed_G1(pairing);
    int G2_LEN_COMPRESSED = pairing_length_in_bytes_compressed_G2(pairing);
    unsigned char data[1000];

    int len = pairing_is_symmetric(pairing) ? G1_LEN_COMPRESSED : G2_LEN_COMPRESSED;
    if (pairing_is_symmetric(pairing)) {
        element_init_G1(pk->v, pairing);
        element_init_G1(pk->params->g, pairing);
    } else {
        element_init_G2(pk->v, pairing);
        element_init_G2(pk->params->g, pairing);
    }
    assert (len < 1000);

    FILE* fd = fopen(filename, "r");
    fread(data, 1, len, fd);
    element_from_bytes_compressed(pk->v, data);

    fread(data, 1, len, fd);
    element_from_bytes_compressed(pk->params->g, data);

    for (int i = 0; i < pk->params->u_size; i++) {
        fread(data, 1, G1_LEN_COMPRESSED, fd);
        element_init_G1(pk->params->u_vec + i, pairing);
        element_from_bytes_compressed(pk->params->u_vec + i, data);
    }

    fclose(fd);
    return pk;
}

// TODO
void free_public_key(struct public_key_t* pk) {

}

void free_private_key(struct private_key_t* pk) {

}

/*
* 11 because 2^33 has 10 digits.
* 1 extra for the null termination.
*/
char int_str[11]; 
void bls_hash_int(
    uint32_t value, 
    struct element_s* output,
    pairing_t pairing
) {
    int len = sprintf(int_str, "%u", value);
    element_from_hash(output, (void*)int_str, len);
}


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