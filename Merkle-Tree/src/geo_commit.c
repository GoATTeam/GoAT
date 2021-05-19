#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

#include "common.h"
#include "aio.h"
#include "tlstime.h"

#include "ccan/minmax/minmax.h"
#include "sodium.h"

bool debug = false;

const int TLS_1_2_ANCHOR = 1;
const int ROUGHTIME_ANCHOR = 2;

struct conf {
    int tls_or_roughtime;
    bool is_benchmark;
    char* anchor_name;
    int n_rounds;
    bool direct;
    char* seed;
    char* filename;
	size_t block_size;
	size_t file_size;
	int nops;
	int queue_depth;
    bool output;
};

static void print_help(FILE *f, char *p) {
	fprintf(f,"Usage: %s -a anchor -t tls_or_roughtime[1/2] -s file_size -n n_rounds [-o (to show output)] [-d (for direct IO)] [-b (for benchmark)] -r seed -f file_name\n", p);
}

static void
parse_conf(struct conf *cnf, int argc, char *argv[]) {

	int opt;
	while ((opt = getopt(argc, argv, "hn:r:s:f:a:t:odb")) != -1) {
		switch (opt) {
			case 'n':
			cnf->n_rounds = atoi(optarg);
			break;

			case 's':
			cnf->file_size = atoi(optarg);
			break;

			case 'd':
			cnf->direct = true;
			break;

			case 'o':
			cnf->output = true;
			break;

			case 'f':
			cnf->filename = optarg;
			break;

			case 'a':
			cnf->anchor_name = optarg;
			break;

			case 'r':
			cnf->seed = optarg;
			break;

            case 't':
            cnf->tls_or_roughtime = atoi(optarg);
            if (cnf->tls_or_roughtime != TLS_1_2_ANCHOR && 
                    cnf->tls_or_roughtime != ROUGHTIME_ANCHOR) {
                        print_help(stdout, argv[0]);
                        exit(0);
                    }
            break;

            case 'b':
            cnf->is_benchmark = true;
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

struct resp {
    int block_num;
    char* data;
};

void init_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        responses[i].data = malloc(cnf->block_size);
    }
}

void free_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        free(responses[i].data);
    }
}

unsigned char* roughtime_get_time(unsigned char* nonce32, char* anchor_name) {
    unsigned char nonce64[64];
    memset(nonce64, 0, 64);
    memcpy(nonce64, nonce32, 32);

    unsigned char hex[128 + 1];
    bytes_to_hex(hex, nonce64, 64);
    hex[128] = '\0';
    printf("[Roughtime] Deriving from %s\n", hex);
    char* tmp_file_for_sig = "rt_sig.tmp";

    {
        char* argv[] = {"rm", tmp_file_for_sig, NULL};
        run(2, argv);
    }

    char transcript_file[50];
    int length = sprintf(transcript_file, "proofs/TRANSCRIPT_RT_");
    for (int i=0; i<4; i++)
        length += sprintf(transcript_file + length, "%02X", nonce64[i]);
    length += sprintf(transcript_file + length, ".out");
    transcript_file[length] = '\0';

    char roughtime_bin[PATH_MAX + 1];
    char *cwd = getcwd(roughtime_bin, PATH_MAX);
    strcat(roughtime_bin, "/../deps/roughenough/target/release/roughenough-client");
    if( access( roughtime_bin, X_OK ) != 0 ) {
        // file doesn't exist
        printf("%s does not exist\n", roughtime_bin);
        exit(0);
    }

    char* argv[] = {roughtime_bin, anchor_name, "2002", 
                    "-t", transcript_file,
                    "-c", hex,
                    NULL};
    run(7, argv);

    FILE* f = fopen(tmp_file_for_sig, "r");
    if (! f) {
        printf("Problem opening file\n");
        exit(0);
    }
    unsigned char* sig = malloc(64);
    fread(sig, 1, 64, f);
    fclose(f);

    bytes_to_hex(hex, sig, 64);
    printf("[Roughtime] Received %s\n", hex);
    return sig;
}

int main(int argc, char* argv[]) {
	struct conf cnf = {
		.seed = "5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2",
		.direct = false,
		.n_rounds = 1,
        .filename = "FILE",
        .block_size = BS,
        .file_size = FS,
        .nops = NUM_CHAL,
        .queue_depth = QUEUE_DEPTH,
        .tls_or_roughtime = TLS_1_2_ANCHOR,
        .is_benchmark = false
	};

	parse_conf(&cnf, argc, argv);

    if (! cnf.anchor_name) { // default anchors
        if (cnf.tls_or_roughtime == 1)
            cnf.anchor_name = "www.google.com";
        else 
            cnf.anchor_name = "roughtime.int08h.com";
    }

    bool benchmarking = cnf.is_benchmark;
	printf("CONF: anchor:%s fs:%luMB n_rounds:%d seed:%s filename:%s bs:%ld nops:%d benchmarking:%d anchor_ping:%d queue_depth:%d direct:%u\n",
	        cnf.anchor_name, cnf.file_size, cnf.n_rounds, cnf.seed, cnf.filename, cnf.block_size, cnf.nops, cnf.is_benchmark, cnf.tls_or_roughtime, cnf.queue_depth, cnf.direct);

    if (strlen(cnf.seed) != 64) {
        printf("The seed must be 32 bytes and input in hex, e.g., 5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2\n");
        return 0;
    }

    unsigned char por_comm[32];
    // Set start to seed for now. In production systems, should be set to random
    hex64_to_bytes(cnf.seed, por_comm);

    // Set the anchor
    unsigned char* (*ping_anchor)(unsigned char*, char*);
    if (cnf.tls_or_roughtime == 1) {
        threaded_tls_init(cnf.anchor_name, cnf.n_rounds + 1);
        ping_anchor = threaded_tls_get_time; // tls_get_time or threaded_tls_get_time
    }
    else if (cnf.tls_or_roughtime == 2)
        ping_anchor = roughtime_get_time;
    else 
        ping_anchor = NULL;

    unsigned char phase1_buf[64 * (cnf.n_rounds + 1)];
    unsigned char* ptr = phase1_buf;
    unsigned char* anchor_hash;
    char buf[65];
    aio_context_t ioctx = 0;
    struct timespec t_begin_anchor, t_end_anchor, t_begin_file, 
                    t_end_file, t_begin_hash, t_end_hash;
    double total_a[cnf.n_rounds+1]; 
    double total_f[cnf.n_rounds];
    double total_h[cnf.n_rounds];
    double total = 0;

    for (int i = 0; i < cnf.n_rounds; i++) {
        // Ping anchor
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_anchor);
        if (ping_anchor) {
            memcpy(ptr, por_comm, 32);
            anchor_hash = ping_anchor(por_comm, cnf.anchor_name);
            memcpy(ptr+32, anchor_hash, 32);
            ptr += 64;
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_anchor);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_file);
        // Derive pseudo-random challenges (for benchmark purpose, we ignore seed)
        uint32_t challenges[cnf.nops];
        if (benchmarking) {
            if (cnf.output)
                printf("[PoR] Deriving randomly\n");
            randombytes_buf(challenges, sizeof challenges);
        } else {
            if (cnf.output) {
                bytes_to_hex(buf, anchor_hash, 32);
                printf("[PoR] Deriving from %s\n", buf);
            }
            randombytes_buf_deterministic(challenges, sizeof(challenges), anchor_hash);
            free(anchor_hash);
        }

        struct resp responses[cnf.nops];
        init_resp(responses, &cnf);
        size_t num_blocks = MB(cnf.file_size) / cnf.block_size;
        for (int i=0; i<cnf.nops; i++) {
            responses[i].block_num = mod(challenges[i], num_blocks);
            if (debug)
                printf("Challenge: %d\t %d\n", challenges[i], responses[i].block_num);
        }

        {
            ioctx = 0;
            struct stat st;
            struct io_op_slab slab;
            int fd = open_file(cnf.filename, cnf.direct);
            if (fstat(fd, &st) == -1) {
                perror("fstat");
                exit(1);
            }
            if (st.st_size != MB(cnf.file_size)) {
                printf("Create file with same size as cnf.file_size. Size mismatch %ld %ld\n", st.st_size, MB(cnf.file_size));
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
                    // responses[index].data = op->buff;
                    memcpy(responses[index].data, op->buff, cnf.block_size);
                    put_io_op(&slab, op);
                }
                completed += to_complete;

                if (debug)
                    printf("in_flight:%zd submitted:%zd (total:%zd) completed:%zd (total:%zd)\n", submitted - completed, to_submit, submitted, to_complete, completed);
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_file);
            io_op_slab_destroy(&slab);
            close(fd);
        }

        t_begin_hash = t_end_file;

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);
        // Compute hash of file blocks
        for (int i=0; i<cnf.nops; i++) {
            if (!benchmarking)
                assert(strtoll(responses[i].data, NULL, 10) == responses[i].block_num);
            crypto_hash_sha256_update(&state, responses[i].data, cnf.block_size);
        }
        crypto_hash_sha256_final(&state, por_comm);
        if (cnf.output) {
            hash_to_string(buf, por_comm);
            printf("[PoR] Commitment: %s\n", buf);
        }

        free_resp(responses, &cnf);
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_hash);
        double time_anchor = (t_end_anchor.tv_sec - t_begin_anchor.tv_sec) * 1000 + 
                            (double) (t_end_anchor.tv_nsec - t_begin_anchor.tv_nsec) / 1000000;
        double time_file = (t_end_file.tv_sec - t_begin_file.tv_sec) * 1000 + 
                            (double) (t_end_file.tv_nsec - t_begin_file.tv_nsec) / 1000000;
        double time_hash = (t_end_hash.tv_sec - t_begin_hash.tv_sec) * 1000 + 
                            (double) (t_end_hash.tv_nsec - t_begin_hash.tv_nsec) / 1000000;
        double time_taken = (t_end_hash.tv_sec - t_begin_anchor.tv_sec) * 1000 + 
                            (double) (t_end_hash.tv_nsec - t_begin_anchor.tv_nsec) / 1000000;
        if (cnf.output) {
            printf("Round:%d, anchor_ping:%f, file_read:%f, hashing:%f\n", i, time_anchor, time_file, time_hash);
        }
        total_a[i] = time_anchor;
        total_f[i] = time_file;
        total_h[i] = time_hash;
        total += time_taken;
    }

    // One last ping
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_anchor);
    if (ping_anchor) {
        memcpy(ptr, por_comm, 32);
        anchor_hash = ping_anchor(por_comm, cnf.anchor_name);
        memcpy(ptr+32, anchor_hash, 32);
        free(anchor_hash);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_anchor);
    double time_anchor = (t_end_anchor.tv_sec - t_begin_anchor.tv_sec) * 1000 + 
                        (double) (t_end_anchor.tv_nsec - t_begin_anchor.tv_nsec) / 1000000;
    total_a[cnf.n_rounds] = time_anchor;
    total += time_anchor;
    if (cnf.output) {
        printf("Round:%d, anchor_ping:%f\n", cnf.n_rounds, time_anchor);
    }

    printf("Time breakdown (averages): anchor_ping:%f file_read:%f hashing:%f\n", 
            mean(total_a, cnf.n_rounds + 1),
            mean(total_f, cnf.n_rounds),
            mean(total_h, cnf.n_rounds));
    printf("Total time for %d rounds is %fms\n", cnf.n_rounds, total);
    printf("Standard deviations: anchor_ping:%f file_read:%f hashing:%f\n", 
            standard_deviation(total_a, cnf.n_rounds + 1),
            standard_deviation(total_f, cnf.n_rounds),
            standard_deviation(total_h, cnf.n_rounds));

    double total_client[cnf.n_rounds];
    for (int i = 0; i < cnf.n_rounds; i++) {
        total_client[i] = total_f[i] + total_h[i];
    }

    printf("[Client-side ops] mean:%f sd:%f\n",
            mean(total_client, cnf.n_rounds),
            standard_deviation(total_client, cnf.n_rounds));

    char *phase1 = "proofs/phase1.out";
    FILE* f = fopen(phase1, "w");
    ptr = phase1_buf;
    char buf64[64];
    for (int i=0; i < cnf.n_rounds + 1; i++) {
        bytes_to_hex(buf64, ptr, 32);
        fwrite(buf64, 1, 64, f);
        fwrite("\n", 1, 1, f);
        ptr += 32;
        bytes_to_hex(buf64, ptr, 32);
        fwrite(buf64, 1, 64, f);
        fwrite("\n", 1, 1, f);
        ptr += 32;
    }
    fclose(f);
    printf("in & out anchor rand written to %s\n", phase1);
    io_destroy(ioctx);
    if (cnf.tls_or_roughtime == TLS_1_2_ANCHOR)
        threaded_tls_close();
    return 0;
}
