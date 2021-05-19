#include <stdio.h>
#include "sodium.h"
#include <time.h>
#include <string.h>

#define point_bytes crypto_core_ed25519_BYTES
#define scalar_bytes crypto_core_ed25519_SCALARBYTES

void bytes_to_hex(char* out, uint8_t* in, int in_len)
{
	size_t i;
	for (i = 0; i < in_len; i++) {
		out += sprintf(out, "%02x", in[i]);
	}
}

int main() {
    int N = 100;
    int testing = 0;

    int ret;
    unsigned char H[N][point_bytes];

    // Generate bases
    for (int i = 0; i < N; i++) {
        crypto_core_ed25519_random(H[i]);
        if (! crypto_core_ed25519_is_valid_point(H[i])) {
            printf("ERROR!!");
            exit(0);
        }
    }
    char hex[point_bytes*2 + 1];
    bytes_to_hex(hex, H[0], point_bytes);
    hex[point_bytes*2] = '\0';
    printf("H0: %s\n", hex);

    // Generate vector of messages
    unsigned char v[N][scalar_bytes];
    for (int i = 0; i < N; i++) {
        crypto_core_ed25519_scalar_random(v[i]);
    }
    bytes_to_hex(hex, v[0], scalar_bytes);
    hex[scalar_bytes*2] = '\0';
    printf("v0: %s\n", hex);

    // Now commit
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    unsigned char intermediate_point[N][point_bytes];
    for (int i = 0; i < N; i++) { // Sum of v_i * H_i
        ret = crypto_scalarmult_ed25519_noclamp(intermediate_point[i], v[i], H[i]);
        if (ret == -1) {
            printf("ERROR #1!");
            exit(0);
        }
    }

    unsigned char com[point_bytes];
    for (int i = 0; i < N; i++) { // Sum of v_i * H_i
        if (i == 0) {
            memcpy(com, intermediate_point[0], point_bytes);
        } else {
            ret = crypto_core_ed25519_add(com, com, intermediate_point[i]);
            if (ret == -1) {
                printf("ERROR #2!");
                exit(0);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    double time_taken = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

    // element_printf("%B\n", com);
    printf("%f for %d scalar multiplications\n", time_taken, N);

    if (testing == 0) {
        exit(0);
    }
    unsigned char vneg[N][scalar_bytes];
    for (int i = 0; i < N; i++) {
        crypto_core_ed25519_scalar_negate(vneg[i], v[i]);
    }

    for (int i = 0; i < N; i++) { // Sum of v_i * H_i
        ret = crypto_scalarmult_ed25519_noclamp(intermediate_point[i], vneg[i], H[i]);
        if (ret == -1) {
            printf("ERROR #1!");
            exit(0);
        }
    }

    unsigned char comneg[point_bytes];
    for (int i = 0; i < N; i++) { // Sum of v_i * H_i
        if (i == 0) {
            memcpy(comneg, intermediate_point[0], point_bytes);
        } else {
            ret = crypto_core_ed25519_add(comneg, comneg, intermediate_point[i]);
            if (ret == -1) {
                printf("ERROR #2!");
                exit(0);
            }
        }
    }

    unsigned char zero1[point_bytes];
    crypto_core_ed25519_add(zero1, com, comneg);

    unsigned char zero2[point_bytes];
    crypto_core_ed25519_sub(zero2, H[1], H[1]);

    ret = sodium_compare(zero1, zero2, point_bytes);
    if (ret != 0) {
        printf("ERROR\n");
    } else {
        printf("Test succeds\n");
    }

}