#include <stdio.h>

#include "relic.h"
#include "relic_bench.h"

#define SIZE 100



void vc() {
	ec_t p, q;
	bn_t k, l, n;
	
	ec_new(p);
	ec_new(q);
	bn_new(k);
	bn_new(n);

	ec_curve_get_ord(n);

        BENCH_BEGIN("ec_mul") {
                bn_rand_mod(k, n);
                ec_rand(p);
                BENCH_ADD(ec_mul(q, p, k));
        }
        BENCH_END;

	ec_t bases[SIZE];
	bn_t coeff[SIZE];

	for (int i = 0; i < SIZE; i++) {
		ec_new(bases[i]);
		bn_new(coeff[i]);
	}

	BENCH_BEGIN("vc_naive") {
		for (int j = 0; j < SIZE; j++) {
			ec_rand(bases[i]);
			bn_rand_mod(coeff[i], n);
		}
	} BENCH_END;

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
	naive();

	core_clean();
        return 0;
}
