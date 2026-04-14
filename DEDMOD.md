void modem_BPSK_demodulate(const float *Y_N, float *L_N,
                            size_t N, float sigma) {
    /*
     * Proper BPSK LLR formula from the course (slide 15):
     *   l_i = 2 * y_i / sigma^2
     *
     * This rescales the received sample into a proper Log-Likelihood Ratio.
     * The sign still tells us bit 0 or 1, but now the magnitude correctly
     * reflects how confident we are, which matters for soft decoding.
     *
     * sigma^2 is the noise variance. A large sigma (noisy channel) makes
     * the LLR magnitude small (less confident). A small sigma (clean channel)
     * makes it large (very confident).
     */
    float sigma2 = sigma * sigma;   /* sigma squared = noise variance */
    for (size_t i = 0; i < N; i++) {
        L_N[i] = (2.0f * Y_N[i]) / sigma2;
    }
}
