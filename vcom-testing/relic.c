#include <stdio.h>

#include <time.h>

#include "relic.h"
#include "relic_bench.h"

#define SIZE 100



void vc() {
	ec_t p, q;
	bn_t k, l, n;

	ec_t t[RLC_EC_TABLE];
        for (int i = 0; i < RLC_EC_TABLE; i++) {
                ec_null(t[i]);
        }
	
	ec_new(p);
	ec_new(q);
	bn_new(k);
	bn_new(n);

	ec_curve_get_ord(n);

        BENCH_BEGIN("ec_mul + ec_add") {
                bn_rand_mod(k, n);
                ec_rand(p);
                BENCH_ADD(ec_mul(q, p, k));
		BENCH_ADD(ec_add(q, p, q));
        }
        BENCH_END;


	BENCH_BEGIN("ec_mul_fix") {
		bn_rand_mod(k, n);
		ec_mul_pre(t, p);
		BENCH_ADD(ec_mul_fix(q, (const ec_t *)t, k));
	}
	BENCH_END;
	
	ec_t bases[SIZE];
	ec_t precomp[SIZE][RLC_EC_TABLE];
	bn_t coeff[SIZE];
	ec_t tmp, com, com2;
	ec_new(tmp);
	ec_new(com);
	ec_new(com2);

	for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < RLC_EC_TABLE; i++) {
                ec_null(precomp[j][i]);
        }
	}

	ec_set_infty(com);
	ec_set_infty(com2);

	for (int i = 0; i < SIZE; i++) {
		ec_new(bases[i]);
		bn_new(coeff[i]);
	}

	for (int j = 0; j < SIZE; j++) {
		ec_rand(bases[j]);
		bn_rand_mod(coeff[j], n);
		ec_mul_pre(precomp[j], bases[j]);
	}

	struct timespec t0, t1, t2;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
	for (int i = 0; i < SIZE; i++) {
		ec_mul(tmp, bases[i], coeff[i]);
		ec_add(com, com, tmp);
	}
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

	for (int i = 0; i < SIZE; i++) {
		ec_mul_fix(tmp, precomp[i], coeff[i]);
		ec_add(com2, com2, tmp);
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
	
	ec_print(com2);
	ec_print(com);

	double exp_time = (t1.tv_sec - t0.tv_sec) * 1000 + 
                        (double) (t1.tv_nsec - t0.tv_nsec) / 1000000;

	double exp_time_2 = (t2.tv_sec - t1.tv_sec) * 1000 + 
                        (double) (t2.tv_nsec - t1.tv_nsec) / 1000000;
	printf("[Naive] %f for %d sized vector\n", exp_time, SIZE);
	printf("[Precompute] %f for %d sized vector\n", exp_time_2, SIZE);

	ec_free(p);
	ec_free(q);
	bn_free(k);
	bn_free(n);
}

int main() {
	if (core_init() != RLC_OK) {
		core_clean();
		return 1;
	}
        conf_print();

	if (ec_param_set_any() != RLC_OK) {
		// RLC_THROW(ERR_NO_CURVE);
		printf("Exiting\n");
		core_clean();
		return 0;
	}

	ec_param_print();
	vc();

	core_clean();
        return 0;
}
