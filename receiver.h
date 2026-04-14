#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdint.h>
#include <stddef.h>


void channel_AWGN_add_noise(const int32_t *X_N, float *Y_N, size_t N, float sigma);

void modem_BPSK_demodulate(const float *Y_N, float *L_N, size_t N, float sigma);

//hard decod

void codec_repetition_hard_decode(const float *L_N, uint8_t *V_K, size_t K, size_t n_reps);

//soft decod

void codec_repetition_soft_decode(const float *L_N, uint8_t *V_K, size_t K, size_t n_reps);

void monitor_check_errors(const uint8_t *U_K, const uint8_t *V_K, size_t K, uint64_t *n_bit_errors, uint64_t *n_frame_errors);

#endif
