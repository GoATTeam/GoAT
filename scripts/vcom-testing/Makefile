all: relic

relic:
	gcc -I/usr/local/include/relic/ -I/usr/local/include/relic/low/ relic.c ../../relic-relic-toolkit-0.5.0/build-gmpbls381/lib/librelic_s.a -lgmp -o relic

clean:
	rm relic
