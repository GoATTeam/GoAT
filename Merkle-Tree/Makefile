CC=gcc
CFLAGS=-I../deps/ccan -I../deps/tlse-geo -Iinclude/ -D_GNU_SOURCE
HEADERS=include/common.h include/sha-256.h include/aio.h
MATH=-lm
SODIUM=-lsodium
THREAD=-lpthread

all: create commit prove verify_por

%.o: src/%.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

tlstime.o: ../deps/tlse-geo/tlstime.c ../deps/tlse-geo/tlse.c
	$(CC) -c -o $@ $<

create: create_file_and_tags.o sha-256.o common.o
	$(CC) -o create create_file_and_tags.o sha-256.o common.o $(MATH)

commit: geo_commit.o common.o tlstime.o
	$(CC) -o commit geo_commit.o common.o tlstime.o $(SODIUM) $(MATH) $(THREAD)

prove: gen_proof_full.o common.o
	$(CC) -o prove gen_proof_full.o common.o $(SODIUM) $(MATH)

verify_por: verify_por_proof.o common.o
	$(CC) -o verify_por verify_por_proof.o common.o $(SODIUM) $(MATH)

.PHONY: clean

clean:
	rm -f *.o create commit verify_por prove rt_sig.tmp proofs/*

file_clean:
	rm FILE TAG

proof_clean:
	rm proofs/*
