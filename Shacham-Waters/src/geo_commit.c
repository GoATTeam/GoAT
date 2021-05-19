#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "aio_helper.h"
#include "audit.h"
#include "common.h"
#include "tlstime.h"
#include "utils.h"
#include "vector_commit.h"

#include "sodium.h"
#include "ccan/minmax/minmax.h"

struct conf {
    char* seed;
    int output; // 0/1
    int rand_queries; // 0/1
    int nops;
    char* filename;
    size_t file_size;
    size_t block_size;
    size_t num_blocks;
    int direct; // 0/1
    int queue_depth;
    char* anchor_name;
    int tls_or_roughtime; // 1/2
    int n_rounds;
    int benchmarking; // 0/1
};

static void print_help(FILE *f, char *p) {
	fprintf(f,"Usage: %s -a anchor -t tls_or_roughtime[1/2] -s file_size -n n_rounds [-o (to show output)] [-d (for direct IO)] [-b (for benchmark)] -r seed -f file_name\n", p);
}

static void parse_conf(struct conf *cnf, int argc, char *argv[]) {
	int opt; // hn:r:s:f:a:t:odb
	while ((opt = getopt(argc, argv, "hn:r:s:a:t:obd")) != -1) {
		switch (opt) {
			case 'n':
			cnf->n_rounds = atoi(optarg);
			break;

			case 's': // Used mainly for benchmark purposes
			cnf->file_size = ((size_t) 1024 * 1024) * atoi(optarg);
            cnf->num_blocks = cnf->file_size / BS;
			break;

			case 'd':
			cnf->direct = true;
			break;

			case 'o':
			cnf->output = true;
			break;

			case 'r':
			cnf->seed = optarg;
			break;

			case 'a':
			cnf->anchor_name = optarg;
			break;

            case 't':
            cnf->tls_or_roughtime = atoi(optarg);
            if (cnf->tls_or_roughtime != TLS_1_2_ANCHOR && 
                    cnf->tls_or_roughtime != ROUGHTIME_ANCHOR && cnf->tls_or_roughtime != 0) {
                        print_help(stdout, argv[0]);
                        exit(0);
                    }
            break;

            case 'b':
            cnf->benchmarking = true;
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

static void init_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        responses[i].data = malloc(cnf->block_size);
    }
}

static void free_resp(struct resp *responses, struct conf* cnf) {
    for (int i=0; i<cnf->nops; i++) {
        free(responses[i].data);
    }
}

static int open_file_helper(struct conf* cnf) {
    struct stat st;
    int fd = open_file(cnf->filename, cnf->direct);
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        exit(1);
    }
    if (st.st_size != cnf->file_size) {
        printf("Create file with same size as cnf->file_size. Size mismatch %ld %ld\n", st.st_size, cnf->file_size);
        exit(0);
    }
    return fd;
}

static void close_file_helper(int fd) {
    close(fd);
}

static void aio_read(struct conf* cnf, 
              struct resp *responses, 
              int fd, 
              aio_context_t* ioctx,
              uint32_t* block_indices
) {
    struct io_op_slab slab;

    if (io_setup(cnf->queue_depth, ioctx) < 0) {
        perror("io_setup");
        exit(1);
    }

    io_op_slab_init(&slab, cnf->queue_depth, cnf->block_size);

    struct iocb *iocb_ptrs[cnf->queue_depth];
    struct io_event io_events[cnf->queue_depth];
    size_t submitted = 0, completed = 0;
    while (completed < cnf->nops) {
        long ret;
        assert(submitted >= completed);
        size_t in_flight = submitted - completed;
        assert(in_flight <= cnf->queue_depth);
        size_t to_submit = min(cnf->queue_depth - in_flight, cnf->nops - submitted);

        for (size_t i=0; i<to_submit; i++) {
            struct io_op *op = get_io_op(&slab);
            assert(op);
            op->num1 = submitted;
            size_t offset = block_indices[submitted];
            io_op_fill_iocb(op, cnf->block_size, offset * cnf->block_size, fd);
            iocb_ptrs[i] = &op->iocb;
            submitted++;
        }

        ret = io_submit(*ioctx, to_submit, iocb_ptrs);
        if (ret < 0) {
            perror("io_submit");
            exit(1);
        } else if (ret != to_submit) {
            fprintf(stderr, "Partial success (%zd instead of %zd). Bailing out\n", ret, to_submit);
            exit(1);
        }

        ret = io_getevents(*ioctx, 0 /* min */, submitted - completed, io_events, NULL);
        if (ret < 0) {
            perror("io_getevents");
            exit(1);
        }

        size_t to_complete = ret;
        for (size_t i=0; i<to_complete; i++) {
            struct io_event *ev = &io_events[i];
            if (ev->res2 != 0 || ev->res != cnf->block_size) {
                fprintf(stderr, "******************** Event returned with res=%lld res2=%lld\n", ev->res, ev->res2);
                exit(1);
            }
            struct io_op *op = (void *)ev->data;
            // printf("Response: %lld\n", op->iocb.aio_offset / cnf->block_size);
            int index = op->num1;
            // responses[index].data = op->buff;
            memcpy(responses[index].data, op->buff, cnf->block_size);
            put_io_op(&slab, op);
        }
        completed += to_complete;

        if (cnf->output)
            printf("in_flight:%zd submitted:%zd (total:%zd) completed:%zd (total:%zd)\n", submitted - completed, to_submit, submitted, to_complete, completed);
    }

    io_op_slab_destroy(&slab);
}

int main(int argc, char** argv) {
	struct conf cnf = {
		.seed = "5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2",
        .output = false,
        .rand_queries = 0,
        .nops = NUM_CHAL,
        .filename = DEFAULT_DATA_FILE,
        .block_size = BS,
        .file_size = FS,
        .num_blocks = NUM_BLOCKS,
        .direct = 0,
        .queue_depth = QUEUE_DEPTH,
        .n_rounds = 1,
        .tls_or_roughtime = TLS_1_2_ANCHOR,
        .benchmarking = false
    };

    parse_conf(&cnf, argc, argv);
    int checks = cnf.benchmarking ? 0 : 1;

    if (! cnf.anchor_name) { // default anchors
        if (cnf.tls_or_roughtime == TLS_1_2_ANCHOR)
            cnf.anchor_name = "www.google.com";
        else if (cnf.tls_or_roughtime == ROUGHTIME_ANCHOR)
            cnf.anchor_name = "roughtime.int08h.com";
        else 
            cnf.anchor_name = "NA";
    }

	printf("CONF: anchor:%s fs:%fMB bench:%d seed:%s filename:%s num_blocks:%ld bs:%ld nops:%d sectors_per_block:%d rounds:%d queue_depth:%d direct:%u\n",
	        cnf.anchor_name, cnf.file_size / (1024.0 * 1024), cnf.benchmarking, cnf.seed, cnf.filename, cnf.num_blocks, cnf.block_size, cnf.nops, NUM_SECTORS, cnf.n_rounds, cnf.queue_depth, cnf.direct);

    if (strlen(cnf.seed) != 64) {
        printf("The seed must be 32 bytes and input in hex, e.g., 5c9f4bf9bbc15bb855f82faedc5a52fc3ab2dfa8ec3ba3ce1b270f31fa0cced2\n");
        return 0;
    }
    struct pairing_s *pairing = (struct pairing_s*)malloc(sizeof(pairing_t));
    pbc_pairing_init(pairing);
    struct vc_param_s* vc_params = init_vc_param(pairing);
    struct response_comm_t* C = init_response_comm_t(NUM_SECTORS, pairing);
    assert(pairing_length_in_bytes_compressed_G1(pairing) < 32);
    struct query_t* query = init_query_t(cnf.nops, pairing);
    struct resp responses[cnf.nops];
    init_resp(responses, &cnf);

    threaded_swcommit_init(cnf.n_rounds, query, responses, C, vc_params);

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
    unsigned char* anchor_hash = por_comm;
    char buf[65];
    aio_context_t ioctx;
    struct timespec t_begin_anchor, t_end_anchor, t_begin_file, 
                    t_end_file, t_begin_com, t_end_com;
    
    double total_a[cnf.n_rounds+1]; 
    double total_f[cnf.n_rounds];
    double total_c[cnf.n_rounds];
    double total = 0;

    int fd = open_file_helper(&cnf);
    for (int i=0; i<cnf.n_rounds; i++) {
        // Ping anchor
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_anchor);
        if (ping_anchor) {
            memcpy(ptr, por_comm, 32);
            if (cnf.output)
                printf("Pinging the anchor now..\n");
            anchor_hash = ping_anchor(por_comm, cnf.anchor_name);
            memcpy(ptr+32, anchor_hash, 32);
            ptr += 64;
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_anchor);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_file);
        ioctx = 0;
        // Derive pseudo-random challenges
        if (cnf.rand_queries) { 
            printf("[PoR] Deriving randomly\n");
            generate_query_random(query, cnf.num_blocks);
        } else {
            if (cnf.output) {
                bytes_to_hex(buf, anchor_hash, 32);
                printf("[PoR] Deriving from %s\n", buf);
            }
            generate_query_deterministic(query, cnf.num_blocks, anchor_hash);
            if (ping_anchor)
                free(anchor_hash);
        }

        if (cnf.output) {
            printf("index 0: %d\n", query->indices[0]);
        }

        // File seek
        aio_read(&cnf, responses, fd, &ioctx, query->indices);

        if (checks) {
            char *ptr, *data;
            for (int k = 0; k < cnf.nops; k++) {
                int i = query->indices[k];
                for (int j = 0; j < NUM_SECTORS; j++) {
                    data = responses[k].data + j * SS;
                    if (strtol(data, &ptr, 10) != i || strtol(ptr, NULL, 10) != j) {
                        perror("File IO error\n");
                        printf("%d %s\n", i, hexstring(data, SS));
                        exit(1);
                    }
                }
            }
            printf("Checks pass\n");
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_file);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t_begin_com);
        // swcommit_serial(query, responses, NUM_SECTORS, SS, C, &cnf);
        threaded_swcommit_run();
        threaded_swcommit_finish();

        if (cnf.output)
            element_printf("com: %B\n", C->vector_com);

        memset(por_comm, 0, 32);
        element_to_bytes_compressed(por_comm, C->vector_com);
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_end_com);

        total_a[i] = timediff(&t_begin_anchor, &t_end_anchor);
        total_f[i] = timediff(&t_begin_file, &t_end_file);
        total_c[i] = timediff(&t_begin_com, &t_end_com);
        total += timediff(&t_begin_anchor, &t_end_com);
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
    total_a[cnf.n_rounds] = timediff(&t_begin_anchor, &t_end_anchor);
    total += timediff(&t_begin_anchor, &t_end_anchor);

    printf("Time breakdown (averages): anchor_ping:%f file_read:%f commit:%f\n", 
            mean(total_a, cnf.n_rounds + 1),
            mean(total_f, cnf.n_rounds),
            mean(total_c, cnf.n_rounds));
    printf("Total time for %d rounds is %fms\n", cnf.n_rounds, total);

    printf("Standard deviations: anchor_ping:%f file_read:%f commit:%f\n", 
            standard_deviation(total_a, cnf.n_rounds + 1),
            standard_deviation(total_f, cnf.n_rounds),
            standard_deviation(total_c, cnf.n_rounds));

    double total_client[cnf.n_rounds];
    for (int i = 0; i < cnf.n_rounds; i++) {
        total_client[i] = total_f[i] + total_c[i];
    }

    printf("[Client-side ops] mean:%f sd:%f\n",
            mean(total_client, cnf.n_rounds),
            standard_deviation(total_client, cnf.n_rounds));

    FILE* f = fopen(DEFAULT_P1_OUT, "w");
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
    printf("in & out anchor rand written to %s\n", DEFAULT_P1_OUT);

    // End...
    free(pairing);
    io_destroy(ioctx);
    if (cnf.tls_or_roughtime == TLS_1_2_ANCHOR)
        threaded_tls_close();
    threaded_swcommit_close();
    close_file_helper(fd);
    free_response_comm_t(C);
    free_resp(responses, &cnf);
    free_query_t(query);
    free_vc_param(vc_params);
}