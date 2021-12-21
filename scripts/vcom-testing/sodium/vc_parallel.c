#include <stdio.h>
#include "sodium.h"
#include <time.h>
#include <string.h>
#include <pthread.h>

#define point_bytes crypto_core_ed25519_BYTES
#define scalar_bytes crypto_core_ed25519_SCALARBYTES

#define N 100 // vector size
#define num_threads 10

int load = (N / num_threads);

int ret;
unsigned char H[N][point_bytes];
unsigned char v[N][scalar_bytes];
unsigned char IP[N][scalar_bytes]; 
unsigned char IS[num_threads][scalar_bytes]; // intermediate sums

// Generate bases and messages
void setup() {
    for (int i = 0; i < N; i++) {
        crypto_core_ed25519_random(H[i]);
        if (! crypto_core_ed25519_is_valid_point(H[i])) {
            printf("ERROR!!");
            exit(0);
        }
    }

    for (int i = 0; i < N; i++) {
        crypto_core_ed25519_scalar_random(v[i]);
    }
}

void *runner(void *t) {
    int i;
    long my_id = (long)t;

    printf("thread %ld. About to start work...\n", my_id);
    int start = my_id * load;
    int end = (my_id + 1) * load; // NOTE: Assumes N is a multiple of load
    for (i = start; i < end; i++) {
        ret = crypto_scalarmult_ed25519_noclamp(IP[i], v[i], H[i]);
        if (ret == -1) {
            printf("ERROR #1!");
            exit(0);
        }
    }

    unsigned char *com = IS[my_id]; // NOTE: Corner case unimplemented
    crypto_core_ed25519_add(com, IP[start], IP[start + 1]);
    for (int i = start + 2; i < end; i++) {
        crypto_core_ed25519_add(com, com, IP[i]);
    }

    pthread_exit(NULL);
}

int main() {
    setup();
    int i;
    pthread_attr_t attr;
    pthread_t threads[num_threads];

    /* For portability, explicitly create threads in a joinable state */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    struct timespec t0, t1, t2;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (i = 0; i < num_threads; i++) {
        long j = i;
        pthread_create(&threads[i], &attr, runner, (void *)j);
    }

    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    unsigned char com[point_bytes];
    crypto_core_ed25519_add(com, IS[0], IS[1]);
    for (int i = 2; i < num_threads; i++) { // Sum of v_i * H_i
        crypto_core_ed25519_add(com, com, IS[i]);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    double exp_time = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

    double total_time = (t2.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t2.tv_nsec - t0.tv_nsec) / 1000000;

    // element_printf("%B\n", com);
    printf("%f (%f) for %d scalar multiplications\n", total_time, exp_time, N);

    pthread_attr_destroy(&attr);
    pthread_exit (NULL);
}
