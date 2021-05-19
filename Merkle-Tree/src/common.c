#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void bytes_to_hex(char* out, uint8_t* in, int in_len)
{
	size_t i;
	for (i = 0; i < in_len; i++) {
		out += sprintf(out, "%02x", in[i]);
	}
}

void hex64_to_bytes(char in[64], char val[32]) {
    const char *pos = in;

    for (size_t count = 0; count < 32; count++) {
        sscanf(pos, "%2hhx", &val[count]);
        pos += 2;
    }
}

void hash_to_string(char string[65], uint8_t hash[32])
{
	size_t i;
	for (i = 0; i < 32; i++) {
		string += sprintf(string, "%02x", hash[i]);
	}
}

long mod(long a, long b)
{
    long r = a % b;
    return r < 0 ? r + b : r;
}

#include <unistd.h>
#include <sys/wait.h>

// To run `ls -hl`, set argv to {"ls", "-hl", NULL}
int run(int ARGC, char* argv[]) {
    if (argv[ARGC]) {
        printf("Last argument not set to NULL\n");
        printf("Argument: %s\n", argv[ARGC]);
        exit(0);
        // throw std::invalid_argument("Last argument not set to NULL");
    }

    int pid, status;
    if ((pid = fork())) {
        waitpid(pid, &status, 0);
    } else {
        execvp(argv[0], argv);
    }
    return status;
}

#include <math.h>

double mean(double data[], int N) {
    double sum = 0.0, mean;
    int i;
    for (i = 0; i < N; ++i) {
        sum += data[i];
    }
    return sum / (double) N;
}

double standard_deviation(double data[], int N) {
    double sum = 0.0, mean, SD = 0.0;
    int i;
    for (i = 0; i < N; ++i) {
        sum += data[i];
    }
    mean = sum / (double) N;
    for (i = 0; i < N; ++i) {
        SD += pow(data[i] - mean, 2);
    }
    return sqrt(SD / N);
}