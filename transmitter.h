#ifndef TRANSMITTER_H
#define TRANSMITTER_H

#include <stdint.h>
#include <stddef.h>


void source_generate(uint8_t *U_K, size_t K);

void codec_repetition_encode(const uint8_t *U_K, uint8_t *C_N, size_t K, size_t n_reps);

void modem_BPSK_modulate(const uint8_t *C_N, int32_t *X_N, size_t N);


#endif
