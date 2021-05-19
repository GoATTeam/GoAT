# SSD benchmarking

One-step benchmarking with fio: Run `fio --randrepeat=1 --ioengine=libaio --direct=1 --gtod_reduce=1 --name=test --filename=test --bs=1k --iodepth=64 --size=1G --readwrite=randread`.

If local benchmark is preferred:

1. Create a large file `./create <FILE_NAME> <SIZE>` (size in MB)
2. Purge cache with `sudo sh -c  "/bin/echo 3 > /proc/sys/vm/drop_caches"`
3. Test random reads `./readn <FILE_NAME> <SIZE>` (or) run `./aio-test -d -f <FILE> -o <BLOCK_SIZE>`

## IO interface comparison

_Thesis_: Safe to assume file size >> cache size.

Block size: 1KB. Machine: YOGA. Switch between buffered, direct IO through O_DIRECT flag. Using simple fread has similar performance characteristic to buffered IO.

_4GB file_
- Normal (with cache drop): file_read:8.709947 hashing:0.562515 total:9.272462
- Direct: file_read:0.782424 hashing:0.517137 total:1.299561

_8GB file_
- Normal (with cache drop): file_read:8.254340 hashing:0.555581 total:8.809922
- Direct: file_read:0.776943 hashing:0.544388 total:1.321331

### Experiment with large file:

_Setup_: c5.4xlarge, 32GB memory, 128GB SSD storage

Created 64GB file. Computed average time per run where each run has 100 random reads (+ some hashing). Total 100 runs.

- Normal: 32.9ms
- libaio-buffered (normal): file_read:38.583383 hashing:0.470637 total:39.054020
- libaio-direct: file_read:1.703197 hashing:0.467923 total:2.171120

## Notes

1. We need an async IO interface to fully exploit the SSD parallelism. The two main options seem to be libaio vs posixaio with the former offering better performance as it is exploits kernel-level support.  

2. We currently use libaio (from libaio-test repo) but it has several problems. Better (more user-friendly) interfaces have been developed very recently, e.g., `io_uring` (Works only on Ubuntu 20.04). Refer to "Direct I/O best practices" in https://people.redhat.com/msnitzer/docs/io-limits.txt for alignment constraints and other advice. Other relevant links: [SO#1](https://stackoverflow.com/questions/34572559/asynchronous-io-io-submit-latency-in-ubuntu-linux/46377629#46377629), [SO#2](https://stackoverflow.com/questions/13407542/is-there-really-no-asynchronous-block-i-o-on-linux/57451551#57451551) and [Rust interface](https://github.com/spacejam/rio). 

3. AWS Notes: IOPS limits based on instance types [link](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ebs-optimized.html). Tested a c5.18xlarge instance. It achieves 64k IOPS (the max EC2 can offer). To do so, use io2 SSD w/ size 128 GB and IOPS limit 64k.

4. libaio vs normal IO: If file size is comparable to that of RAM size, normal IO will outperform libaio. libaio is extremely useful only when the file size is big enough; the difference starts to show at approx twice the RAM size.

5. fallocate seems to have a weird affect by which IO on the file is fastened. Prefer using fio's file creation or some other method.

<!-- 6. Time to fetch the timestamp from a TLS 1.2 server is about 4-6x the RTT. (We could run an experiment with our own server to determine how much of that is due to load or server processing.) -->

### Useful Commands

1. Sort CSV from command line: `sort -k3 -n -t, <CSV>`

