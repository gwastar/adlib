#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "array.h"

int main(void)
{
	int *arr1 = NULL;
	int n = 300000;
	for (int i = 0; i < n; i++) {
		array_add(arr1, i);
	}

	array_ordered_deleten(arr1, 0, 2);

	for (size_t i = 0; i < array_length(arr1); i++) {
		for (size_t k = 0; k < (i + 1) / 2; k++) {
			if (arr1[i] % arr1[k] == 0) {
				array_ordered_delete(arr1, i);
				i--;
				break;
			}
		}
	}

	printf("%zu primes between 0 and %d\n", array_length(arr1), n);
	// printf("array capacity: %zu\n", array_capacity(arr1));
	array_free(arr1);

#if 0
	int K = 10000;
	double o[3] = {0};
	for (int k = 0; k < K; k++) {
		int N = 10 * (k + 1);
		ARRAY_GROWTH_FACTOR_NUMERATOR = 2;
		ARRAY_GROWTH_FACTOR_DENOMINATOR = 1;
		for (int i = 0; i < N; i++) {
			array_add(arr1, i);
		}
		o[0] += (double)array_capacity(arr1) / array_len(arr1);
		array_free(arr1);

		ARRAY_GROWTH_FACTOR_NUMERATOR = 3;
		ARRAY_GROWTH_FACTOR_DENOMINATOR = 2;
		for (int i = 0; i < N; i++) {
			array_add(arr1, i);
		}
		o[1] += (double)array_capacity(arr1) / array_len(arr1);
		array_free(arr1);

		ARRAY_GROWTH_FACTOR_NUMERATOR = 8;
		ARRAY_GROWTH_FACTOR_DENOMINATOR = 5;
		for (int i = 0; i < N; i++) {
			array_add(arr1, i);
		}
		o[2] += (double)array_capacity(arr1) / array_len(arr1);
		array_free(arr1);
	}
	o[0] /= K;
	o[1] /= K;
	o[2] /= K;
	printf("%g\n", o[0]);
	printf("%g\n", o[1]);
	printf("%g\n", o[2]);

	return 0;
#endif
}
