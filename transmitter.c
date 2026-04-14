#include "transmitter.h"
#include <stdlib.h>
#include <stdio.h>


void source_generate(uint8_t *U_K, size_t K) {

	for (size_t i = 0; i < K; i++) {
	       U_K[i] = rand() % 2; //random 0 or 1
	}
}

void codec_repetition_encode(const uint8_t *U_K, uint8_t *C_N, size_t K, size_t n_reps) {
	for (size_t j = 0; j < n_reps; j++) {
		for (size_t i = 0; i < K; i++) {
			C_N[j * K + i] = U_K[i];
		}
	}
}


void modem_BPSK_modulate(const uint8_t *C_N, int32_t *X_N, size_t N) {
	for (size_t i = 0; i < N; i++) {
		X_N[i] = (C_N[i] == 0) ? +1 : -1;
	}
}


