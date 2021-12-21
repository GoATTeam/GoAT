// Vector commitments in pairing-friendly curves

#include <pbc/pbc.h>

#include <time.h>
#include <pthread.h>

#define SIZE 100

void test_ec_commit_3x(pairing_t pairing) {
    if (SIZE % 3 != 0) {
        printf("Will throw gadbad\n");
    }
    // Initialization: Create base, values 
    struct element_s bases[SIZE];
    element_t com, tmp, com1;
    for (int i=0; i<SIZE; i++) {
        element_init_G1(&bases[i], pairing);
        element_random(&bases[i]);
    }
    element_init_G1(com, pairing);
    element_init_G1(com1, pairing);
    element_init_G1(tmp, pairing);
    element_set1(com);
    element_set1(com1);

    struct element_s val[SIZE];
    for (int i=0; i<SIZE; i++) {
        element_init_Zr(&val[i], pairing);
        element_random(&val[i]);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    // Commit to val using base
    for (int i=0; i<SIZE / 3; i++) { // com = base_1^val_1 x base_2^val_2 x ...
        element_pow3_zn(tmp, &bases[3*i], &val[3*i], &bases[3*i+1], &val[3*i+1], &bases[3*i+2], &val[3*i+2]);
        // element_pow_zn(tmp, &bases[i], &val[i]);
        element_mul(com, com, tmp);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    double time_taken = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

    // element_printf("%B\n", com);
    printf("=======3x=====\n");
    printf("%fms\n", time_taken);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    // Commit to val using base
    for (int i=0; i<SIZE; i++) { // com = base_1^val_1 x base_2^val_2 x ...
        // element_pow3_zn(tmp, &bases[i], &val[i], &bases[i+1], &val[i+1], &bases[i+2], &val[i+2]);
        element_pow_zn(tmp, &bases[i], &val[i]);
        element_mul(com1, com1, tmp);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    time_taken = (t1.tv_sec - t0.tv_sec) * 1000 + 
                    (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;
    printf("=======Naive=====\n");
    printf("%fms\n", time_taken);

    if (element_cmp(com, com1) != 0) {
        printf("gadbad\n");
    }
}

void test_ec_commit_precomp(pairing_t pairing) {
    printf("=======Precompute=====\n");

    struct element_s bases[SIZE];
    struct element_pp_s bases_precomp[SIZE];
    element_t com, tmp, com1;
    for (int i=0; i<SIZE; i++) {
        element_init_G1(&bases[i], pairing);
        element_random(&bases[i]);
        element_pp_init(&bases_precomp[i], &bases[i]);
    }
    element_init_G1(com, pairing);
    element_init_G1(tmp, pairing);
    element_init_G1(com1, pairing);
    element_set1(com);
    element_set1(com1);

    struct element_s val[SIZE];
    for (int i=0; i<SIZE; i++) {
        element_init_Zr(&val[i], pairing);
        element_random(&val[i]);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    // Commit to val using base
    for (int i=0; i<SIZE; i++) { // com = base_1^val_1 x base_2^val_2 x ...
        element_pp_pow_zn(tmp, &val[i], &bases_precomp[i]);
        element_mul(com, com, tmp);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    double time_taken = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

    // element_printf("%B\n", com);
    printf("%fms for %d sized vector commit\n", time_taken, SIZE);

    for (int i=0; i<SIZE; i++) { // com = base_1^val_1 x base_2^val_2 x ...
        element_pow_zn(tmp, &bases[i], &val[i]);
        element_mul(com1, com1, tmp);
    }

    if (element_cmp(com, com1) != 0) {
        printf("gadbad\n");
    }
}


#define num_threads 10

int load = (SIZE / num_threads);
int ret;

struct element_pp_s bases_precomp[SIZE];
struct element_s val[SIZE];
struct element_s IP[SIZE];
struct element_s IS[num_threads];

void setup_for_vc_parallel(pairing_t pairing) {
    struct element_s bases[SIZE];
    for (int i=0; i<SIZE; i++) {
        element_init_G1(&bases[i], pairing);
        element_random(&bases[i]);
        element_pp_init(&bases_precomp[i], &bases[i]);
    }

    for (int i=0; i<SIZE; i++) {
        element_init_Zr(&val[i], pairing);
        element_random(&val[i]);
        element_init_G1(&IP[i], pairing);
    }

    for (int i=0; i<num_threads; i++) {
        element_init_G1(&IS[i], pairing);
        element_set1(&IS[i]);
    }
}

void *runner(void *t) {
    int i;
    long my_id = (long)t;

    printf("thread %ld. About to start work...\n", my_id);
    int start = my_id * load;
    int end = (my_id + 1) * load; // NOTE: Assumes N is a multiple of load
    for (i = start; i < end; i++) {
        element_pp_pow_zn(&IP[i], &val[i], &bases_precomp[i]);
    }

    struct element_s *com = &IS[my_id]; // NOTE: Corner case unimplemented
    for (int i = start; i < end; i++) {
        element_mul(com, com, &IP[i]);
    }

    pthread_exit(NULL);
}

void test_ec_commit_precomp_parallel(pairing_t pairing) {
    printf("=======Precompute + Parallelize=====\n");
    int i;
    setup_for_vc_parallel(pairing);

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

    element_t com; // NOTE: Corner case unimplemented
    element_init_G1(com, pairing);
    element_set1(com);
    for (int i = 0; i < num_threads; i++) {
        element_mul(com, com, &IS[i]);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    double exp_time = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

    double total_time = (t2.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t2.tv_nsec - t0.tv_nsec) / 1000000;

    // element_printf("%B\n", com);
    printf("%f (%f) for %d sized vector commit\n", total_time, exp_time, SIZE);

    pthread_attr_destroy(&attr);
    pthread_exit (NULL);
}

/*@manual test
Initializes pairing from file specified as first argument, or from standard
input if there is no first argument.
*/
static inline void pbc_demo_pairing_init(pairing_t pairing, int argc, char **argv) {
  char s[16384];
  FILE *fp = stdin;

  if (argc > 1) {
    fp = fopen(argv[1], "r");
    if (!fp) pbc_die("error opening %s", argv[1]);
  } else {
    printf("No input given. Choosing the default type f curve\n");
    fp = fopen("../Shacham-Waters/param/f.param", "r");
    if (!fp) pbc_die("error opening %s", argv[1]);
  }
  size_t count = fread(s, 1, 16384, fp);
  if (!count) pbc_die("input error");
  fclose(fp);

  if (pairing_init_set_buf(pairing, s, count)) pbc_die("pairing init failed");
}


int main(int argc, char **argv) {
    pairing_t pairing;

    pbc_demo_pairing_init(pairing, argc, argv);

    test_ec_commit_3x(pairing);

    test_ec_commit_precomp(pairing);

    test_ec_commit_precomp_parallel(pairing);
}
