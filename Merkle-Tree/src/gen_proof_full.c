#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"
#include "aio.h"

#include "ccan/minmax/minmax.h"
#include "sodium.h"

struct conf {
    char* filename;
    bool direct;
    char* seed;
    size_t file_size;
	size_t block_size;
	int nops;
	int queue_depth;
    char* tagfilename;
    int levels;
};

static void print_help(FILE *f, char *p) {
	fprintf(f,
            "Usage: %s -s file_size -f file_name -r seed [-d (for direct IO)]\n"
            "File size in MB. Nearest size such that the number of blocks is a power of 2 will be chosen.\n", 
            p);
}

static void
parse_conf(struct conf *cnf, int argc, char *argv[]) {

	int opt;
	while ((opt = getopt(argc, argv, "hf:s:r:t:d")) != -1) {
		switch (opt) {
			case 's':
			cnf->file_size = atoi(optarg);
			break;

			case 'd':
			cnf->direct = true;
			break;

			case 'r':
			cnf->seed = optarg;
			break;

			case 'f':
			cnf->filename = optarg;
			break;

			case 't':
			cnf->tagfilename = optarg;
			break;

			case 'h':
			print_help(stdout, argv[0]);
			exit(0);

			default:
			print_help(stderr, argv[0]);
			exit(1);
		}
	}
}


bool debug = false;
uint8_t *root, *level1left, *level1right;

// offset(level, 0/1)
int offset(int x, int y, int levels) { 
    return (x * 32 * 2) + (y * 32); 
}

struct resp {
    int block_num;
    char* data;
    int* merkle_indices; // [LEVELS]
    uint8_t* merkle_path; // [LEVELS][2][32]; // [LEVELS-1][0] has root whereas [LEVELS-1][1] is unused
};

void init_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        responses[i].data = malloc(cnf->block_size);
        responses[i].merkle_indices = malloc(cnf->levels * sizeof( *(responses[i].merkle_indices)));
        responses[i].merkle_path = malloc(cnf->levels * 2 * 32 * sizeof(uint8_t));
    }
}

void free_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        free(responses[i].data);
        free(responses[i].merkle_indices);
        free(responses[i].merkle_path);
    }
}

int compute_tag_indices(struct resp *responses, int levels, struct conf* cnf) {
    int num_seeks = 0;
    int tot = pow(2, levels) - 1;
    // Compute merkle paths now
    for (int i=0; i<cnf->nops; i++) {
        int block_num = responses[i].block_num;
        int cur = block_num;
        int index = 0;
        for (int j=levels-1; j>0; j--) {
            int l, r;
            if (cur % 2 == 0) {
                l = cur;
                r = cur + 1;
            } else {
                r = cur;
                l = cur - 1;
            }

            // printf("j: %d, l: %d, r: %d\n", j, index+l, index+r);
            // Fetch hashes here
            responses[i].merkle_indices[j] = index+l;
            num_seeks++;
            // fseek(tag_file, (index + l)*32, SEEK_SET);
            // fread(hl, 1, 32, tag_file);
            // fread(hr, 1, 32, tag_file);

            cur /= 2;
            index += pow(2, j);
        }
        assert(index == tot-1);
        responses[i].merkle_indices[0] = index;
    }
    num_seeks++; // root
    return num_seeks;
}

int main(int argc, char* argv[]) {
	struct conf cnf = {
		.seed = "5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2",
		.direct = false,
        .filename = "FILE",
        .tagfilename = "TAG",
        .block_size = BS,
        .file_size = FS,
        .nops = NUM_CHAL,
        .queue_depth = QUEUE_DEPTH
	};
	parse_conf(&cnf, argc, argv);

    size_t num_blocks = MB(cnf.file_size) / cnf.block_size;
    int levels = 1 + log2(num_blocks);
    cnf.levels = levels;

	printf("CONF: filename:%s file_size: %luMB, block_size:%ld tag: %s nops:%d direct:%u levels:%d seed:%s\n",
	        cnf.filename, cnf.file_size, cnf.block_size, cnf.tagfilename, cnf.nops, cnf.direct, cnf.levels, cnf.seed);

    unsigned char hash_string[65];
    // drop_caches();
    struct timespec t_begin_file, t_end_file, t_begin_tag, t_end_tag, t_begin_hash, t_end_hash;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_file);
    if (strlen(cnf.seed) != 64) {
        printf("The seed must be 32 bytes and input in hex, e.g., 5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2\n");
        return 0;
    }
    unsigned char seed_bytes[32];
    hex64_to_bytes(cnf.seed, seed_bytes);

    // Derive random challenges
    uint32_t challenges[cnf.nops];
    randombytes_buf_deterministic(challenges, sizeof(challenges), seed_bytes);

    struct resp responses[cnf.nops];
    init_resp(responses, &cnf);
    for (int i=0; i<cnf.nops; i++) {
        responses[i].block_num = mod(challenges[i], num_blocks);
        if (debug)
            printf("Challenge: %d\t %d\n", challenges[i], responses[i].block_num);
    }
    // printf("%ld\n", sizeof(R));

    aio_context_t ioctx = 0, ioctx_tag = 0;
    {
        struct stat st;
        struct io_op_slab slab;
        int fd = open_file(cnf.filename, false);
        if (fstat(fd, &st) == -1) {
            perror("fstat");
            exit(1);
        }
        if (st.st_size != MB(cnf.file_size)) {
            printf("Create file with same size as file_size. Size mismatch %ld %ld\n", st.st_size, MB(cnf.file_size));
            assert(false);
        }

        if (io_setup(cnf.queue_depth, &ioctx) < 0) {
            perror("io_setup");
            exit(1);
        }

        io_op_slab_init(&slab, cnf.queue_depth, cnf.block_size);

        struct iocb *iocb_ptrs[cnf.queue_depth];
        struct io_event io_events[cnf.queue_depth];
        size_t submitted = 0, completed = 0;
        while (completed < cnf.nops) {
            long ret;
            assert(submitted >= completed);
            size_t in_flight = submitted - completed;
            assert(in_flight <= cnf.queue_depth);
            size_t to_submit = min(cnf.queue_depth - in_flight, cnf.nops - submitted);

            for (size_t i=0; i<to_submit; i++) {
                struct io_op *op = get_io_op(&slab);
                assert(op);
                op->num1 = submitted;
                size_t offset = responses[submitted].block_num;
                io_op_fill_iocb(op, cnf.block_size, offset * cnf.block_size, fd);
                iocb_ptrs[i] = &op->iocb;
                submitted++;
            }

            ret = io_submit(ioctx, to_submit, iocb_ptrs);
            if (ret < 0) {
                perror("io_submit");
                exit(1);
            } else if (ret != to_submit) {
                fprintf(stderr, "Partial success (%zd instead of %zd). Bailing out\n", ret, to_submit);
                exit(1);
            }

            ret = io_getevents(ioctx, 0 /* min */, submitted - completed, io_events, NULL);
            if (ret < 0) {
                perror("io_getevents");
                exit(1);
            }

            size_t to_complete = ret;
            for (size_t i=0; i<to_complete; i++) {
                struct io_event *ev = &io_events[i];
                if (ev->res2 != 0 || ev->res != cnf.block_size) {
                    fprintf(stderr, "******************** Event returned with res=%lld res2=%lld\n", ev->res, ev->res2);
                    exit(1);
                }
                struct io_op *op = (void *)ev->data;
                // printf("Response: %lld\n", op->iocb.aio_offset / cnf.block_size);
                int index = op->num1;
                memcpy(responses[index].data, op->buff, cnf.block_size);
                put_io_op(&slab, op);
            }
            completed += to_complete;

            if (debug)
                printf("in_flight:%zd submitted:%zd (total:%zd) completed:%zd (total:%zd)\n", submitted - completed, to_submit, submitted, to_complete, completed);
        }
        io_op_slab_destroy(&slab);
        close(fd);
        // printf("Finished reading FILE\n");
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_file);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_tag);
    root = &responses[0].merkle_path[offset(0, 0, levels)];
    level1left = &responses[0].merkle_path[offset(1, 0, levels)];
    level1right = &responses[0].merkle_path[offset(1, 1, levels)];

    int num_seeks_for_tags = compute_tag_indices(responses, levels, &cnf);
    num_seeks_for_tags = (cnf.nops * (levels - 2)) + 2/2 + 1;
    printf("Total num seeks: %d\n", num_seeks_for_tags);
    {
        struct stat st;
        struct io_op_slab slab;
        // printf("Reading TAG\n");
        int fdtag = open_file("TAG", false);
        if (fstat(fdtag, &st) == -1) {
            perror("fstat");
            exit(1);
        }

        if (io_setup(cnf.queue_depth, &ioctx_tag) < 0) {
            perror("io_setup");
            exit(1);
        }

        int block_size = FILE_ALIGNMENT;
        io_op_slab_init(&slab, cnf.queue_depth, block_size);

        // uint32_t chals[num_seeks_for_tags];
        // randombytes_buf_deterministic(chals, sizeof(challenges), seed_bytes);

        struct iocb *iocb_ptrs[cnf.queue_depth];
        struct io_event io_events[cnf.queue_depth];
        size_t submitted = 0, completed = 0;
        int chal = 0, level = levels-1;

        while (completed < num_seeks_for_tags) {
            long ret;
            assert(submitted >= completed);
            size_t in_flight = submitted - completed;
            assert(in_flight <= cnf.queue_depth);
            size_t to_submit = min(cnf.queue_depth - in_flight, num_seeks_for_tags - submitted);

            if (debug)
                printf("in_flight:%zd submitted:%zd (total:%zd) completed: (total:%zd)\n", submitted - completed, to_submit, submitted, completed);

            for (size_t i=0; i<to_submit; i++) {
                struct io_op *op = get_io_op(&slab);
                assert(op);
                op->num1 = chal;
                op->num2 = level;
                // printf("Chal: %d, Level: %d\n", chal, level);
                assert(chal < cnf.nops);
                assert(level < levels);
                assert(level >= 0);
                assert(responses[chal].merkle_indices[level] % 2 == 0);
                size_t offset =  (responses[chal].merkle_indices[level] / 2);
                // if (debug)
                // printf("Chal: %d, Level: %d, Offset: %ld\n", chal, level, offset);
                io_op_fill_iocb(op, block_size, offset * block_size, fdtag);
                iocb_ptrs[i] = &op->iocb;
                submitted++;
                if ((level > 2) || (level > 0 && chal == 0)) {
                    level--;
                } else {
                    chal++;
                    level = levels - 1;
                }
            }

            ret = io_submit(ioctx_tag, to_submit, iocb_ptrs);
            if (ret < 0) {
                perror("io_submit");
                exit(1);
            } else if (ret != to_submit) {
                fprintf(stderr, "Partial success (%zd instead of %zd). Bailing out\n", ret, to_submit);
                exit(1);
            }

            ret = io_getevents(ioctx_tag, 0 /* min */, submitted - completed, io_events, NULL);
            if (ret < 0) {
                perror("io_getevents");
                exit(1);
            }

            size_t to_complete = ret;
            for (size_t i=0; i<to_complete; i++) {
                struct io_event *ev = &io_events[i];
                if (ev->res2 != 0 || ev->res != block_size) {
                    fprintf(stderr, "******************** Event returned with res=%lld res2=%lld\n", ev->res, ev->res2);
                    exit(1);
                }
                struct io_op *op = (void *)ev->data;
                int rec_chal = op->num1;
                int rec_level = op->num2;
                if (debug)
                    printf("Response offset: %lld\n", op->iocb.aio_offset / block_size);
                // hash_to_string(hash_string, op->buff);
                // printf("%s\n", hash_string);
                // hash_to_string(hash_string, op->buff+32);
                // printf("%s\n", hash_string);
                if (rec_level == 0) {
                    memcpy(root, op->buff, 32);
                } else if (rec_level == 1) {
                    memcpy(level1left, op->buff, 32);
                    memcpy(level1right, op->buff+32, 32);
                } else {
                    memcpy(&responses[rec_chal].merkle_path[offset(rec_level, 0, levels)], op->buff, 32);
                    memcpy(&responses[rec_chal].merkle_path[offset(rec_level, 1, levels)], op->buff+32, 32);
                }
                put_io_op(&slab, op);
            }
            completed += to_complete;

            if (debug)
                printf("in_flight:%zd submitted:%zd (total:%zd) completed:%zd (total:%zd)\n", submitted - completed, to_submit, submitted, to_complete, completed);
        }
        io_op_slab_destroy(&slab);
        close(fdtag);
        // printf("Finished reading TAG\n");
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_tag);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_hash);

    hash_to_string(hash_string, root);
    printf("H(root): %s\n", hash_string);

    unsigned char out[32];
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    // Compute merkle paths now
    for (int i=0; i<cnf.nops; i++) {
        assert(strtoll(responses[i].data, NULL, 10) == responses[i].block_num);
        crypto_hash_sha256_update(&state, responses[i].data, cnf.block_size);
        for (int j=levels-1; j>=0; j--) {
            // printf("j: %d, l: %d, r: %d\n", j, index+l, index+r);
            // Fetch hashes here
            if (j == 0) {
                crypto_hash_sha256_update(&state, root, 32);
                crypto_hash_sha256_update(&state, root, 32);
            } else if (j == 1) {
                crypto_hash_sha256_update(&state, level1left, 32);
                crypto_hash_sha256_update(&state, level1right, 32);
            } else {
                crypto_hash_sha256_update(&state, &responses[i].merkle_path[offset(j, 0, levels)], 32);
                crypto_hash_sha256_update(&state, &responses[i].merkle_path[offset(j, 1, levels)], 32);
            }
        }
    }
    crypto_hash_sha256_final(&state, out);
    hash_to_string(hash_string, out);
    printf("Hash(proof): %s\n", hash_string);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_hash);
    double time_file = (t_end_file.tv_sec - t_begin_file.tv_sec) * 1000 + 
                        (double) (t_end_file.tv_nsec - t_begin_file.tv_nsec) / 1000000;
    double time_tag = (t_end_tag.tv_sec - t_begin_tag.tv_sec) * 1000 + 
                        (double) (t_end_tag.tv_nsec - t_begin_tag.tv_nsec) / 1000000;
    double time_hash = (t_end_hash.tv_sec - t_begin_hash.tv_sec) * 1000 + 
                        (double) (t_end_hash.tv_nsec - t_begin_hash.tv_nsec) / 1000000;
    double total_time = (t_end_hash.tv_sec - t_begin_file.tv_sec) * 1000 + 
                        (double) (t_end_hash.tv_nsec - t_begin_file.tv_nsec) / 1000000;
    printf("[Time breakdown] File: %f, Tag: %f, Hash: %f, Total: %f\n", time_file, time_tag, time_hash, total_time);

    // Write proof out
    FILE *proof_file;
    char proof[50];
    int length = sprintf(proof, "proofs/PROOF_POR_");
    for (int i=0; i<4; i++)
        length += sprintf(proof + length, "%02x", seed_bytes[i]);
    length += sprintf(proof + length, ".out");
    proof[length] = '\0';
    
    proof_file = fopen(proof, "w");
    printf("Creating %s...\t", proof);
    for (int i=0; i<cnf.nops; i++) {
        fwrite(responses[i].data, 1, cnf.block_size, proof_file);
        for (int j=levels-1; j>=0; j--) {
            if (j == 0) {
                fwrite(root, 1, 32, proof_file);
                fwrite(root, 1, 32, proof_file);
            } else if (j == 1) {
                fwrite(level1left, 1, 32, proof_file);
                fwrite(level1right, 1, 32, proof_file);
            } else {
                fwrite(&responses[i].merkle_path[offset(j, 0, levels)], 1, 32, proof_file);
                fwrite(&responses[i].merkle_path[offset(j, 1, levels)], 1, 32, proof_file);
            }
        }
    }
    free_resp(responses, &cnf);
    fclose(proof_file);
    io_destroy(ioctx);
    io_destroy(ioctx_tag);
    printf("Done\n");
    return 0;
}