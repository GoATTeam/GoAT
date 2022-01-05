all: relic

relic: relic.c
	gcc -I/usr/local/include/relic/ -I/usr/local/include/relic/low/ relic.c ../relic-relic-toolkit-0.5.0/build-gmp-bls381/lib/librelic_s.a -lgmp -o relic

pbc: pbc.c
	gcc pbc.c -lgmp -lpthread -lpbc -o pbc

clean:
	rm pbc relic
