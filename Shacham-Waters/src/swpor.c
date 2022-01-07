#include <stdio.h>

#include "audit.h"
#include "common.h"
#include "file.h"
#include "utils.h"

int main() {
    relic_pairing_init();

    struct file_t* file_info = read_file(DEFAULT_DATA_FILE, NUM_BLOCKS, NUM_SECTORS, SS);
    printf("File read successfully!\n");

    import_tag(DEFAULT_TAG_FILE, file_info);
    printf("Tags read successfully!\n");

    struct public_key_t* pk = import_pk(DEFAULT_PK_FILE, NUM_SECTORS);
    if (! pk) {
        printf("Error\n");
        exit(0);
    }
    printf("Public key read successfully!\n");

    struct query_t* Q = init_query_t(NUM_CHAL);
    //unsigned char seed[32];
    //hex64_to_bytes("5c9f4bf9b2c15bb815f82faedc5a52fc3cb2dfa83c3ba3cf1b270f31fa0cced2", seed);
    //generate_query_deterministic(Q, NUM_BLOCKS, seed, 32);
    generate_query_random(Q, NUM_BLOCKS);
    printf("Query indices generated!\n");

    struct query_response_t* R = query_with_full_file(Q, file_info);
    printf("Query response computed!\n");

    if (verify(Q, R, pk)) {
        printf("Success\n");
    } else {
        printf("Fail\n");
    }
}
