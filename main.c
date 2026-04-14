#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <unistd.h>



#include "receiver.h"
#include "transmitter.h"

int main(int argc, char *argv[]) {
	if (argc !=4) {
		fprintf(stderr, "usage: %s <K> <n_reps> <sigma>\n", argv[0]);
		fprintf(stderr, "K : number of src bits\n");
		fprintf(stderr, "n_reps : repetitions\n");
		fprintf(stderr, "sigma strength of the noise\n");
		return 1;
	}

	size_t K 	= (size_t)atoi(argv[1]);
	size_t n_reps 	= (size_t)atoi(argv[2]);
	float sigma 	= atoi(argv[3]);


	if (K == 0 || n_reps == 0) {
		fprintf(stderr, "ERROR K and n_reps"); 
		return 1;
	}


	size_t N = K * n_reps;

	srand((unsigned int)time(NULL)); //random generation 
	
	uint8_t *U_K = malloc(K * sizeof(uint8_t));
	uint8_t *C_N = malloc(N * sizeof(uint8_t));
	int32_t *X_N = malloc(N * sizeof(int32_t));
	float 	*Y_N = malloc(N * sizeof(float));  
	float 	*L_N = malloc(N * sizeof(float));  
	uint8_t *V_K = malloc(N * sizeof(uint8_t));  

	if (!U_K || !C_N || !X_N || !Y_N || !L_N || !V_K) {
		fprintf(stderr, "malloc failing\n");
		free(U_K);
		free(C_N);
		free(X_N);
		free(Y_N);
		free(L_N);
		free(V_K);
		return 1;
	}


	//generate src bits
	
	source_generate(U_K, K);
	printf("== U_K source  K=%zu == \n", K);
	for (size_t i = 0; i < K; i++)
		printf("U_K[%zu] = %u\n", i, U_K[i]);

	//encoder 
	
	codec_repetition_encode(U_K, C_N, K, n_reps);
	printf("\n== C_N for N=%zu, reps=%zu ==\n", N, n_reps);
	for (size_t i = 0; i < N; i++)
		printf("C_N[%zu] = %u\n", i, C_N[i]);

	//modulation
	
	modem_BPSK_modulate(C_N, X_N, N);
	printf("\n == X_N ==\n");
	for (size_t i = 0; i < N; i++)
		printf("X_N[%zu] = %+d\n", i, X_N[i]);

	// channel and noise
	
	channel_AWGN_add_noise(X_N, Y_N, N, sigma);
	printf("Y_N noisy signal \n");
	for (size_t i = 0; i < N; i++)
		printf("Y_N[%zu] = %+.3f \n", i, Y_N[i]);


	// receiver part RX 
	// demodulate 
	
	modem_BPSK_demodulate(Y_N, L_N, N, sigma);
	printf("L_N demodulated LLRS \n");
	for (size_t i = 0; i < N; i++)
		printf("L_N[%zu] = %+.3f \n", i, L_N[i]);

	// soft decode
	codec_repetition_soft_decode(L_N, V_K, K, n_reps);
	printf("V_K decoded bits \n");
	for (size_t i = 0; i < K; i++)
	       printf("%u", V_K[i]);	

	//monitor
	uint64_t n_bit_errors = 0;
        uint64_t n_frame_errors = 0;

	monitor_check_errors(U_K, V_K, K, &n_bit_errors, &n_frame_errors);	  printf("\nbit errors %lu \nframe errors %lu\n", n_bit_errors, n_frame_errors);	
	
	free(U_K);
	free(C_N);
	free(X_N);
	free(Y_N);
	free(L_N);
	free(V_K);
	
	return 0;

}






