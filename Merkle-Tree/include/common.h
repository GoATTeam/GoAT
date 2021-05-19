#include <stdint.h>
#include "math.h"

#define FILE_ALIGNMENT 512
#define BS 1024
#define MB(x) (x * 1024 * 1024)
#define NUM_CHAL 100
#define _POWER 10 // x => 2^x MB
#define FS (int) pow(2, _POWER)
#define LEVELS (11 + _POWER) // 2->12, 4->13, 8->14, 2^k->12+(k-1)
#define NUM_BLOCKS (MB(FS) / BS)
#define QUEUE_DEPTH 32

void hex64_to_bytes(char in[64], char val[32]);
void bytes_to_hex(char* out, uint8_t* in, int in_len);
void hash_to_string(char string[65], uint8_t hash[32]);
long mod(long a, long b);
int run(int ARGC, char* argv[]);

double mean(double data[], int N);

double standard_deviation(double data[], int N);