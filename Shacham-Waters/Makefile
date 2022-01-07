CC=gcc
OTHERFLAGS=-O0
CFLAGS=-Iinclude/ -I../deps/ccan/ -I../deps/tlse-geo/ -I/usr/local/include/relic/ -I/usr/local/include/relic/low/
HEADERS=include/*
MATH=-lm
SODIUM=-lsodium
THREAD=-lpthread
PBC=-lpbc
GMP=-lgmp
RELIC=../deps/relic/build/lib/librelic_s.a

ALLEXTERNAL=$(RELIC) $(SODIUM) $(GMP) $(THREAD) $(MATH)

BINARIES=create swpor commit prove_and_verify
all: $(BINARIES)

%.o: src/%.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(OTHERFLAGS)

tlstime.o: ../deps/tlse-geo/tlstime.c ../deps/tlse-geo/tlse.c
	$(CC) -c -o $@ $< $(OTHERFLAGS)

create: create_file_and_tags.o utils.o file.o vector_commit.o
	$(CC) -o $@ $^ $(RELIC) $(PBC) $(GMP) $(MATH) $(THREAD) $(OTHERFLAGS)

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
	rm -rf test*.dat data/* proofs/* $(BINARIES) test_io *.o

test: utils.o file.o vector_commit.o
	$(CC) -c -o test_io.o test/io.c $(CFLAGS) $(OTHERFLAGS)
	$(CC) -o test_io test_io.o $^ $(RELIC) $(ALLEXTERNAL) $(OTHERFLAGS)

.PHONY: test 
