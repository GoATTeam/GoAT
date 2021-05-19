#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "sha-256.h"

#include "ccan/list/list.h"

bool debug = false;

struct conf {
    char* filename;
    size_t file_size;
	size_t block_size;
	bool gen_tag;
    char* tagfilename;
};

static void print_help(FILE *f, char *p) {
	fprintf(f,
            "Usage: %s -t tag_file_name -f file_name -s file_size -b block_size\n"
            "File size in MB. Nearest size such that the number of blocks is a power of 2 will be chosen.\n", 
            p);
}

static void
parse_conf(struct conf *cnf, int argc, char *argv[]) {

	int opt;
	while ((opt = getopt(argc, argv, "hf:b:s:t")) != -1) {
		switch (opt) {
			case 'b':
			cnf->block_size = atol(optarg);
			break;

			case 's':
			cnf->file_size = atol(optarg);
			break;

			case 't':
            cnf->gen_tag = true;
			break;

			case 'f':
			cnf->filename = optarg;
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

struct hash {
    uint8_t val[32];
    struct list_node lnode;
};

struct all_hashes {
	struct list_head head;
	size_t total_num;
};

static struct hash* alloc_hash() {
    struct hash* ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

    return ret;
}

static void free_hash(struct hash* h) {
    free(h);
}

static void free_all_hashes(struct all_hashes *ah) {
	size_t nops = 0;
	struct hash *h;
	while ((h = list_pop(&ah->head, struct hash, lnode))) {
		free_hash(h);
		nops++;
	}
	if (nops != ah->total_num)
		fprintf(stderr, "Error: leaked io ops (freed:%zd, total:%zd)\n", nops, ah->total_num);

	ah->total_num = 0;
}

int main(int argc, char* argv[]) {
	struct conf cnf = {
        .filename = "FILE",
        .file_size = FS,
        .block_size = BS,
		.gen_tag = false,
        .tagfilename = "TAG"
	};

	parse_conf(&cnf, argc, argv);

	printf("CONF: filename:%s file_size:%luMB block_size:%luKB gen_tag:%u tagfilename:%s\n",
	        cnf.filename, cnf.file_size, cnf.block_size, cnf.gen_tag, cnf.tagfilename);

    if (cnf.file_size <= 0 || MB(cnf.file_size) % cnf.block_size != 0) {
		printf("Improper file size: %ld!\n", cnf.file_size);
        printf("USAGE: ./create_file_and_tags FILE_SIZE\n"
               "File size in MB. Nearest size such that the number of blocks is a power of 2 will be chosen. Two files FILE and TAG will be created.\n");
        return 0;
    }
    size_t _num_blocks = MB(cnf.file_size) / cnf.block_size;
    int nearest_power_of_2 = log2(_num_blocks);
    size_t num_blocks = pow(2, nearest_power_of_2);
    size_t fs_bytes = num_blocks * cnf.block_size;
    printf("Creating %s with %ldMB size and %ld (2**%d) blocks\n", cnf.filename, fs_bytes / (1024 * 1024), num_blocks, nearest_power_of_2);

    FILE* fd = fopen(cnf.filename, "w");
    if (!fd) {
        printf("Cannot create file, exiting\n");
        exit(1);
    }

    int fdtag;
    if (cnf.gen_tag) {
        printf("Creating TAG\n");
        fdtag = open(cnf.tagfilename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    }

    char str[cnf.block_size];
    char hash_string[65];

    struct all_hashes* baseH;
    if (cnf.gen_tag) {
        baseH = malloc(sizeof(*baseH));
        list_head_init(&baseH->head);
    }
    char zerobuf64[FILE_ALIGNMENT - 64];
    char zerobuf32[FILE_ALIGNMENT - 32];

    for (size_t i=0; i<num_blocks; i++) {
        // set the first few bytes to index
        memset(str, 0, cnf.block_size);
        sprintf(str, "%ld", i);
        fwrite(str, 1, cnf.block_size, fd);
        // strcpy(curptr, str);
        if (strtol(str, NULL, 10) != i) {
            perror("File IO error\n");
            exit(1);
        }

        if (!cnf.gen_tag)
            continue;

        // compute hash of the file block
        struct hash* h = alloc_hash();
		if (!h) {
			fprintf(stderr, "alloc_hash failed\n");
			abort();
		}
        calc_sha_256(h->val, str, cnf.block_size);
        if (debug) {
            hash_to_string(hash_string, h->val);
            printf("%ld %s\n", i, hash_string);
        }
        // now add it to the tag file
        write(fdtag, h->val, 32);
        if (i % 2 != 0) {
            write(fdtag, zerobuf64, sizeof zerobuf64);
        }
        // fwrite(h->val, 1, 32, tag_file);
        list_add_tail(&baseH->head, &h->lnode);
        // printf("%ld Hash: %s\n", strtol(curptr, NULL, 10), hash_string);
    }
    fclose(fd);
    if (!cnf.gen_tag) {
        return 0;
    }

    baseH->total_num = num_blocks;
    printf("Level %d over. Written %ld hashes\n", nearest_power_of_2, num_blocks);

    // build rest of the merkle tree now
    int levels = nearest_power_of_2;
    char hashblock[64];
    struct all_hashes *curH, *prevH = baseH;
    for (int level = levels-1; level>=0; level--) {
        curH = malloc(sizeof(*curH));
        list_head_init(&curH->head);
        struct hash *cur, *prev;
        int i=0;
        int j=0;
        list_for_each(&prevH->head, cur, lnode) {
            if (i % 2 == 1) {
                prev = list_prev(&prevH->head, cur, lnode);
                if (!prev) {
                    fprintf(stderr, "list_prev failed %d\n", i);
                    abort();
                }
                memcpy(hashblock, prev->val, 32);
                memcpy(hashblock+32, cur->val, 32);

                struct hash* h = alloc_hash();
                if (!h) {
                    fprintf(stderr, "alloc_hash failed\n");
                    abort();
                }
                calc_sha_256(h->val, hashblock, 64);
                if (debug) {
                    hash_to_string(hash_string, h->val);
                    printf("%d %s\n", level, hash_string);
                }
                write(fdtag, h->val, 32);
                if (j%2 == 1) {
                    write(fdtag, zerobuf64, sizeof zerobuf64);
                }
                if (level == 0) {
                    assert(j==0);
                    write(fdtag, zerobuf32, sizeof zerobuf32);
                }
                list_add_tail(&curH->head, &h->lnode);
                j++;
            }
            i++;
        }
        assert(i % 2 == 0);
        printf("Level %d over. Written %d hashes\n", level, i/2);
        curH->total_num = i/2;
        free_all_hashes(prevH);
        free(prevH);
        prevH = curH;
    }

    free_all_hashes(curH);
    free(curH);
    close(fdtag);
}
