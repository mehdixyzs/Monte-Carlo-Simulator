/*
 * main.c
 * ------
 * Monte Carlo simulator for a BPSK + repetition code communication chain.
 *
 * Implements Algorithm 1 from the course slides (slide 22) exactly:
 *
 *   For each Eb/N0 SNR point:
 *     1. Convert Eb/N0 to sigma (noise std deviation)
 *     2. Simulate frames in a loop until f_max frame errors collected
 *        - source_generate      -> U_K (random bits)
 *        - repetition_encode    -> C_N (repeated bits)
 *        - BPSK_modulate        -> X_N (+1/-1 symbols)
 *        - AWGN_add_noise       -> Y_N (noisy samples)
 *        - BPSK_demodulate      -> L_N (LLRs)
 *        - repetition_decode    -> V_K (decoded bits)
 *        - monitor_check_errors -> update be, fe counters
 *     3. Output BER and FER for this SNR point as a CSV line
 *
 * Command line arguments (parsed with getopt):
 *   -m : min Eb/N0 in dB (first SNR point, included)
 *   -M : max Eb/N0 in dB (last SNR point, included)
 *   -s : step between SNR points in dB
 *   -e : max frame errors to collect per SNR point (f_max)
 *   -K : number of information bits per frame
 *   -N : codeword size (must be a multiple of K, n_reps = N/K)
 *   -D : decoder type, "rep-hard" or "rep-soft"
 *
 * Usage example:
 *   ./simulator -m 0 -M 15 -s 1 -e 100 -K 32 -N 128 -D "rep-hard"
 *
 * Redirect output to CSV:
 *   ./simulator -m 0 -M 15 -s 1 -e 100 -K 32 -N 128 -D "rep-hard" > sim1.csv
 */

#include <stdio.h>      /* printf, fprintf, fflush */
#include <stdlib.h>     /* malloc, free, atof, atoi */
#include <stdint.h>     /* uint8_t, uint64_t */
#include <string.h>     /* strcmp, strncpy */
#include <math.h>       /* powf, sqrtf, log10f */
#include <time.h>       /* clock_gettime, CLOCK_MONOTONIC */
#include <unistd.h>     /* getopt */

#include "transmitter.h"
#include "receiver.h"

int main(int argc, char *argv[]) {

    /* ----------------------------------------------------------
     * Default simulation parameters.
     * All of these are overridden by command line arguments.
     * ---------------------------------------------------------- */
    float    min_snr = 0.0f;       /* min Eb/N0 in dB */
    float    max_snr = 10.0f;      /* max Eb/N0 in dB */
    float    step    = 1.0f;       /* SNR step in dB */
    uint64_t f_max   = 100;        /* frame errors to collect per SNR point */
    size_t   K       = 32;         /* number of source (information) bits */
    size_t   N       = 128;        /* codeword size = K * n_reps */
    char     decoder[16] = "rep-soft"; /* decoder type */

    /* ----------------------------------------------------------
     * Parse command line arguments using getopt.
     *
     * getopt() scans argv[] looking for option flags like -m, -K, etc.
     * The string "m:M:s:e:K:N:D:" defines which flags are valid.
     * A colon after a letter means that flag requires an argument.
     *
     * optarg: global pointer to the argument string for current option.
     * getopt returns -1 when no more options are found.
     *
     * atof() converts a string to float  (e.g. "0.5" -> 0.5f)
     * atoi() converts a string to int    (e.g. "32"  -> 32)
     * ---------------------------------------------------------- */
    int opt;
    while ((opt = getopt(argc, argv, "m:M:s:e:K:N:D:")) != -1) {
        switch (opt) {
            case 'm': min_snr = atof(optarg);                         break;
            case 'M': max_snr = atof(optarg);                         break;
            case 's': step    = atof(optarg);                         break;
            case 'e': f_max   = (uint64_t)atoi(optarg);              break;
            case 'K': K       = (size_t)atoi(optarg);                break;
            case 'N': N       = (size_t)atoi(optarg);                break;
            case 'D': strncpy(decoder, optarg, sizeof(decoder) - 1); break;
            default:
                /* unknown option: print usage and exit with error */
                fprintf(stderr,
                    "Usage: %s -m min -M max -s step -e f_max "
                    "-K bits -N codeword -D [rep-hard|rep-soft]\n",
                    argv[0]);
                return 1;
        }
    }

    /* ----------------------------------------------------------
     * Validate that N is a multiple of K.
     *
     * The repetition encoder repeats the K-bit block n_reps times
     * to produce a codeword of N bits. This only works if N/K is
     * a whole number.
     *
     * Example: K=32, N=128 -> n_reps = 4 (valid)
     *          K=32, N=100 -> 100 % 32 = 4 (invalid, error)
     * ---------------------------------------------------------- */
    if (N % K != 0) {
        fprintf(stderr,
            "Error: N (%zu) must be a multiple of K (%zu).\n", N, K);
        return 1;
    }

    /* n_reps: how many times the K-bit block is repeated */
    size_t n_reps = N / K;

    /* coding rate R = K/N (fraction of bits that carry information) */
    float R = (float)K / (float)N;

    /* ----------------------------------------------------------
     * Validate and select the decoder type.
     *
     * use_soft = 1 -> soft decoding (average LLRs, better performance)
     * use_soft = 0 -> hard decoding (majority vote, simpler but worse)
     * ---------------------------------------------------------- */
    int use_soft;
    if (strcmp(decoder, "rep-soft") == 0)
        use_soft = 1;
    else if (strcmp(decoder, "rep-hard") == 0)
        use_soft = 0;
    else {
        fprintf(stderr,
            "Error: -D must be 'rep-hard' or 'rep-soft', got '%s'\n",
            decoder);
        return 1;
    }

    /* ----------------------------------------------------------
     * Seed the random number generator.
     *
     * srand() initializes the pseudo-random number generator (PRNG).
     * Using time(NULL) as seed ensures different results each run.
     * rand() is then used inside source_generate() and gaussian().
     * ---------------------------------------------------------- */
    srand((unsigned int)time(NULL));

    /* ----------------------------------------------------------
     * Allocate all simulation buffers ONCE before the main loop.
     *
     * Allocating inside the SNR loop or frame loop would be extremely
     * slow (millions of malloc/free calls per simulation).
     * We allocate once and reuse the same buffers for every frame.
     *
     * Buffer overview:
     *   U_K : K bits  — source bits generated by source_generate()
     *   C_N : N bits  — encoded bits after repetition encoder
     *   X_N : N int32 — BPSK symbols (+1 or -1) after modulator
     *   Y_N : N float — noisy received samples after AWGN channel
     *   L_N : N float — LLRs (log-likelihood ratios) after demodulator
     *   V_K : K bits  — decoded bits output by the channel decoder
     * ---------------------------------------------------------- */
    uint8_t *U_K = malloc(K * sizeof(uint8_t));
    uint8_t *C_N = malloc(N * sizeof(uint8_t));
    int32_t *X_N = malloc(N * sizeof(int32_t));
    float   *Y_N = malloc(N * sizeof(float));
    float   *L_N = malloc(N * sizeof(float));
    uint8_t *V_K = malloc(K * sizeof(uint8_t));

    /* check every malloc succeeded (returns NULL on failure) */
    if (!U_K || !C_N || !X_N || !Y_N || !L_N || !V_K) {
        fprintf(stderr, "Error: malloc failed — not enough memory.\n");
        /* free whatever was allocated before failing */
        free(U_K); free(C_N); free(X_N);
        free(Y_N); free(L_N); free(V_K);
        return 1;
    }

    /* ----------------------------------------------------------
     * Print CSV header line.
     *
     * This line describes the 10 columns of output.
     * When redirecting to a file (./simulator ... > sim1.csv),
     * this becomes the first line of the CSV file.
     * ---------------------------------------------------------- */
    printf("Eb/N0(dB),Es/N0(dB),sigma,be,fe,fn,BER,FER,time(s),time/frame(us)\n");

    /* ----------------------------------------------------------
     * OUTER LOOP: iterate over each SNR (Eb/N0) point.
     *
     * We go from min_snr to max_snr with steps of size `step`.
     * The +1e-6f epsilon prevents floating point rounding from
     * skipping the last SNR point (e.g. 15.0 becoming 14.9999...).
     *
     * This is the "foreach Eb/N0 in the SNRs" from Algorithm 1.
     * ---------------------------------------------------------- */
    for (float snr = min_snr; snr <= max_snr + 1e-6f; snr += step) {

        /* ------------------------------------------------------
         * Convert Eb/N0 (dB) -> sigma (noise standard deviation)
         *
         * Step 1: dB to linear
         *   EbN0_lin = 10^(Eb/N0_dB / 10)
         *
         * Step 2: apply coding rate and modulation order
         *   From slide 13: Es/N0 = Eb/N0 + 10*log10(R * bs)
         *   For BPSK: bs = 1, so:
         *   Es/N0_lin = Eb/N0_lin * R    (where R = K/N)
         *
         * Step 3: compute sigma from Es/N0
         *   From slide 14: sigma = sqrt(1 / (2 * Es/N0_lin))
         *   This comes from sigma^2 = N0/2 and Es/N0 = Es/N0_lin
         *
         * A high Eb/N0 (good SNR) -> small sigma (little noise)
         * A low  Eb/N0 (bad  SNR) -> large sigma (lots of noise)
         * ------------------------------------------------------ */
        float EbN0_lin = powf(10.0f, snr / 10.0f);
        float EsN0_lin = EbN0_lin * R;
        float EsN0_dB  = 10.0f * log10f(EsN0_lin);
        float sigma    = sqrtf(1.0f / (2.0f * EsN0_lin));

        /* error and frame counters for this SNR point, start at 0 */
        uint64_t n_bit_errors   = 0;  /* total bit errors across all frames */
        uint64_t n_frame_errors = 0;  /* total frame errors across all frames */
        uint64_t n_frames       = 0;  /* total frames simulated */

        /* start wall-clock timer for this SNR point */
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        /* ------------------------------------------------------
         * INNER LOOP: simulate frames until f_max frame errors.
         *
         * This is the "do ... while fe < fmax" from Algorithm 1.
         *
         * We don't know how many frames fn we'll need — it depends
         * on the SNR, code, and channel. At high SNR, very few frames
         * produce errors, so fn becomes very large (simulation is slow).
         * At low SNR, almost every frame has an error, so fn is small.
         *
         * Each iteration of this loop = one simulated frame of K bits.
         * ------------------------------------------------------ */
        while (n_frame_errors < f_max) {

            /* === TRANSMITTER (TX) === */

            /* Generate K random source bits into U_K */
            source_generate(U_K, K);

            /* Repeat the K-bit block n_reps times -> C_N of size N */
            codec_repetition_encode(U_K, C_N, K, n_reps);

            /* Map each bit to a BPSK symbol: 0->+1, 1->-1 -> X_N */
            modem_BPSK_modulate(C_N, X_N, N);

            /* === CHANNEL === */

            /* Add AWGN noise: Y_N[i] = X_N[i] + sigma * N(0,1) */
            channel_AWGN_add_noise(X_N, Y_N, N, sigma);

            /* === RECEIVER (RX) === */

            /* Convert noisy samples to LLRs: L_N[i] = 2*Y_N[i]/sigma^2 */
            modem_BPSK_demodulate(Y_N, L_N, N, sigma);

            /* Decode: choose hard or soft based on -D argument */
            if (use_soft)
                /* Soft: average LLRs across repetitions, then decide */
                codec_repetition_soft_decode(L_N, V_K, K, n_reps);
            else
                /* Hard: hard-decide each LLR, then majority vote */
                codec_repetition_hard_decode(L_N, V_K, K, n_reps);

            /* === MONITOR === */

            /* Compare U_K (sent) vs V_K (decoded), update error counters */
            monitor_check_errors(U_K, V_K, K,
                                 &n_bit_errors, &n_frame_errors);

            n_frames++;  /* one more frame simulated */
        }

        /* stop the timer */
        clock_gettime(CLOCK_MONOTONIC, &t_end);

        /* compute elapsed time in seconds (use double for precision) */
        double elapsed = (double)(t_end.tv_sec  - t_start.tv_sec)
                       + (double)(t_end.tv_nsec - t_start.tv_nsec) * 1e-9;

        /* ------------------------------------------------------
         * Compute BER and FER from the counters.
         *
         * BER (Bit Error Rate):
         *   = total bit errors / total bits transmitted
         *   = n_bit_errors / (n_frames * K)
         *   Represents the probability that any single bit is wrong.
         *
         * FER (Frame Error Rate):
         *   = total frame errors / total frames
         *   = n_frame_errors / n_frames
         *   Represents the probability that an entire frame has >= 1 error.
         *
         * time_per_frame_us:
         *   Average time to simulate one frame, in microseconds.
         *   Useful for benchmarking the Jetson's throughput.
         * ------------------------------------------------------ */
        double BER = (double)n_bit_errors
                   / ((double)n_frames * (double)K);
        double FER = (double)n_frame_errors / (double)n_frames;
        double time_per_frame_us = (elapsed / (double)n_frames) * 1e6;

        /* ------------------------------------------------------
         * Print one CSV line for this SNR point.
         *
         * Format: 10 comma-separated values per line.
         * %lu prints uint64_t values (unsigned long).
         * %.6e prints floats in scientific notation (e.g. 1.234567e-05).
         * ------------------------------------------------------ */
        printf("%.2f,%.4f,%.6f,%lu,%lu,%lu,%.6e,%.6e,%.6f,%.4f\n",
               snr,              /* col 1: Eb/N0 in dB */
               EsN0_dB,          /* col 2: Es/N0 in dB */
               sigma,            /* col 3: noise std deviation */
               n_bit_errors,     /* col 4: total bit errors */
               n_frame_errors,   /* col 5: total frame errors (= f_max) */
               n_frames,         /* col 6: total frames simulated */
               BER,              /* col 7: bit error rate */
               FER,              /* col 8: frame error rate */
               elapsed,          /* col 9: total time for this SNR point (s) */
               time_per_frame_us /* col 10: avg time per frame (microseconds) */
        );

        /* flush stdout immediately so we see progress line by line,
         * especially important when redirecting output to a CSV file */
        fflush(stdout);
    }

    /* ----------------------------------------------------------
     * Free all allocated memory before exiting.
     *
     * Although the OS reclaims memory on program exit anyway,
     * freeing explicitly avoids false positives in Valgrind/ASan
     * and is good C practice.
     * ---------------------------------------------------------- */
    free(U_K);
    free(C_N);
    free(X_N);
    free(Y_N);
    free(L_N);
    free(V_K);

    return 0;
}
