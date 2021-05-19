CC=gcc
OTHERFLAGS=-O0
CFLAGS=-Iinclude/ -I../deps/ccan/ -I../deps/tlse-geo/
HEADERS=include/*
MATH=-lm
SODIUM=-lsodium
THREAD=-lpthread
PBC=-lpbc
GMP=-lgmp

ALLEXTERNAL=$(SODIUM) $(PBC) $(GMP) $(THREAD) $(MATH)

BINARIES=create swpor commit prove_and_verify
all: $(BINARIES)

%.o: src/%.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(OTHERFLAGS)

tlstime.o: ../deps/tlse-geo/tlstime.c ../deps/tlse-geo/tlse.c
	$(CC) -c -o $@ $< $(OTHERFLAGS)

create: create_file_and_tags.o utils.o file.o vector_commit.o
	$(CC) -o $@ $^ $(PBC) $(GMP) $(MATH) $(THREAD) $(OTHERFLAGS)

swpor: swpor.o file.o utils.o audit.o vector_commit.o
	$(CC) -o $@ $^ $(ALLEXTERNAL) $(OTHERFLAGS)

commit: geo_commit.o utils.o audit.o vector_commit.o tlstime.o
	$(CC) -o $@ $^ $(ALLEXTERNAL) $(OTHERFLAGS)

prove_and_verify: prove_and_verify.o utils.o audit.o file.o vector_commit.o
	$(CC) -o $@ $^ $(ALLEXTERNAL) $(OTHERFLAGS)

clean:
	rm $(BINARIES) *.o

proof_clean:
	rm proofs/*

full_clean: 
	rm -rf data/* proofs/* $(BINARIES) *.o
