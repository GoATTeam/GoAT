Code to illustrate linux-aio. 
See: http://kkourt.io/blog/2017/10-14-linux-aio.html

- `aio-example.c`: simple example of the interface
- `aio-test.c`: utility that submits and completes async IO requests

For example:

Create a file:
```
$ fallocate -l 1073741824 FILE
```

Issue random writes in the file and print how many operations are in flight after
calling `io_submit()` and `io_getevents()`.
```
$ ./aio-test  -f FILE  -w 100 | grep -o 'in_flight:[^ ]\+' | sort -V | uniq -c
      1 in_flight:0
   3909 in_flight:1
    514 in_flight:2
     99 in_flight:3
     29 in_flight:4
      5 in_flight:5
      1 in_flight:6
      3 in_flight:7
```

Do the same. (My guess it that there is more concurrency now because the
file structure is already initialized.)
```
$ ./aio-test  -f FILE  -w 100 | grep -o 'in_flight:[^ ]\+' | sort -V | uniq -c
     39 in_flight:0
      1 in_flight:1
      3 in_flight:2
    153 in_flight:8
    186 in_flight:9
    165 in_flight:10
      1 in_flight:11
      2 in_flight:12
      3 in_flight:13
      8 in_flight:14
      9 in_flight:15
     18 in_flight:16
     32 in_flight:17
     75 in_flight:18
    112 in_flight:19
    270 in_flight:20
   2000 in_flight:21
   3156 in_flight:22
    248 in_flight:23
    225 in_flight:24
     40 in_flight:25
     23 in_flight:26
    134 in_flight:27
     95 in_flight:28
    140 in_flight:29
   1472 in_flight:30
  33613 in_flight:31
3321019 in_flight:32
```

The same, but with reads:
```
$ ./aio-test  -f FILE   | grep -o 'in_flight:[^ ]\+' | sort -V | uniq -c   
     54 in_flight:0
      1 in_flight:1
    295 in_flight:4
    210 in_flight:5
      1 in_flight:7
      1 in_flight:8
    103 in_flight:12
    317 in_flight:13
     83 in_flight:14
      3 in_flight:15
      2 in_flight:16
      9 in_flight:17
     10 in_flight:18
     33 in_flight:19
    138 in_flight:20
    236 in_flight:21
    370 in_flight:22
    249 in_flight:23
    446 in_flight:24
   2606 in_flight:25
   3045 in_flight:26
   4349 in_flight:27
    336 in_flight:28
    213 in_flight:29
    413 in_flight:30
  28616 in_flight:31
5888215 in_flight:32
```
