#include <errno.h>
#include <fcntl.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ccan/list/list.h"

struct io_op {
	char             *buff;
    int              num1;
    int              num2;
	struct iocb      iocb;
	struct list_node lnode;
};

struct io_op_slab {
	struct list_head io_ops;
	size_t total_nops;
};

static inline long
io_setup(unsigned maxevents, aio_context_t *ctx) {
    return syscall(SYS_io_setup, maxevents, ctx);
}

static inline long
io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp) {
    return syscall(SYS_io_submit, ctx, nr, iocbpp);
}

static inline long
io_getevents(aio_context_t ctx, long min_nr, long nr,
                 struct io_event *events, struct timespec *timeout) {
    return syscall(SYS_io_getevents, ctx, min_nr, nr, events, timeout);
}

static inline int
io_destroy(aio_context_t ctx) {
    return syscall(SYS_io_destroy, ctx);
}

static struct io_op *
alloc_io_op(size_t buff_size) {
	int err;
	struct io_op *ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;
	err = posix_memalign((void **)&ret->buff, 4096, buff_size);
	if (err) {
		free(ret);
		return NULL;
	}

	return ret;
}

static void
free_io_op(struct io_op *io) {
	free(io->buff);
	free(io);
}

static void
io_op_slab_init(struct io_op_slab *slab, size_t nops, size_t buff_size) {
	list_head_init(&slab->io_ops);
	for (size_t i=0; i<nops; i++) {
		struct io_op *op = alloc_io_op(buff_size);
		if (!op) {
			fprintf(stderr, "alloc_io_op failed\n");
			abort();
		}
		list_add_tail(&slab->io_ops, &op->lnode);
	}
	slab->total_nops = nops;
}

static void
io_op_slab_destroy(struct io_op_slab *slab) {
	size_t nops = 0;
	struct io_op *op;
	while ((op = list_pop(&slab->io_ops, struct io_op, lnode))) {
		free_io_op(op);
		nops++;
	}

	if (nops != slab->total_nops)
		fprintf(stderr, "Error: leaked io ops (freed:%zd, total:%zd)\n", nops, slab->total_nops);

	slab->total_nops = 0;
}


static struct io_op *
get_io_op(struct io_op_slab *slab) {
	return list_pop(&slab->io_ops, struct io_op, lnode);
}

static void
put_io_op(struct io_op_slab *slab, struct io_op *op) {
	list_add(&slab->io_ops, &op->lnode);
}

static inline void
io_op_fill_iocb(struct io_op *op, size_t num_bytes, size_t offset, int fd) {
    // printf("num_bytes: %ld, offset: %ld, fd: %d\n", num_bytes, offset, fd);
	memset(&op->iocb, 0, sizeof(op->iocb));
	op->iocb.aio_buf        = (uintptr_t)op->buff;
	op->iocb.aio_data       = (uintptr_t)op;
	op->iocb.aio_fildes     = fd;
	op->iocb.aio_lio_opcode = IOCB_CMD_PREAD;
	op->iocb.aio_reqprio    = 0;
	op->iocb.aio_nbytes     = num_bytes;
	op->iocb.aio_offset     = offset;
}

static int open_file(char* filename, bool is_direct) {
    int fd;
    int oflags = O_RDONLY;
    if (is_direct) {
        oflags |= __O_DIRECT;
    }
	if ((fd = open(filename, oflags, S_IRUSR | S_IWUSR)) == -1) {
		perror(filename);
		exit(1);
	}

	return fd;
}

static void
drop_caches(void) {
	int fd;
	const char fname[] = "/proc/sys/vm/drop_caches";

	char data[16];
	snprintf(data, sizeof(data), "%d\n", 3);

	if ((fd = open(fname, O_WRONLY)) == -1) {
		fprintf(stderr, "Failed to drop caches: open %s: %s (requires root)\n", fname, strerror(errno));
		exit(1);
	}

	size_t data_len = strnlen(data, sizeof(data));
	if (write(fd, data, data_len) != data_len) {
		fprintf(stderr, "Failed to drop caches: write %s: %s\n", fname, strerror(errno));
		exit(1);
	}

	close(fd);
	return;
}
