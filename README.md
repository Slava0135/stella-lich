# Stella Lich

Garbage Collector & Runtime for [Stella](https://fizruk.github.io/stella/)

## Overview

Mark and Sweep implementation (with optional incremental mode, using write barriers).

## Install

Install `cmake`, `gcc`, `g++`, `make`.

Run to configure project:

```sh
CC=gcc CXX=g++ cmake -S . -B build
```

To build all targets:

```sh
cmake --build build --target all .
```

To build library:

```sh
cmake --build build --target lich .
```

Library artifact `liblich.a` will be located in `./out` directory.

## Tests

To build tests:

```sh
cmake --build build --target tests .
```

To run tests:

```sh
cd build/test/
ctest
```

## Usage

Compile program C source generated from (<https://fizruk.github.io/stella/playground/>) or docker `docker run -i fizruk/stella compile < PROGRAM.stella > PROGRAM.c`

Then run:

```sh
gcc -std=c11 -c PROGRAM.c -o PROGRAM.o
g++ -std=c++20 PROGRAM.o liblich.a -o PROGRAM.out
```

To run program:

```sh
echo "42" | PROGRAM.out
```

## Debug

When built in DEBUG mode (-g), every operation will be logged in console (e.g. `collect` and `allocate`).

Additional debug flags can be passed when compiling for extra debug info.

```sh
gcc -DSTELLA_DEBUG -DSTELLA_GC_STATS -DSTELLA_RUNTIME_STATS ...
```

Example GC dump:

```text
STATS
+----------------------------+------------------+-------------------+
| COLLECTIONS (full)         |                  |          1 cycles |
+----------------------------+------------------+-------------------+
| MEMORY USED (max)          |        152 bytes |          7 blocks |
+----------------------------+------------------+-------------------+
| MEMORY USED                |        120 bytes |          5 blocks |
| MEMORY USED (w/o metadata) |         80 bytes |                   |
| MEMORY FREE                |        136 bytes |          3 blocks |
| MEMORY FREE (w/o metadata) |        112 bytes |                   |
+----------------------------+------------------+-------------------+
| READS / WRITES             |          0 reads |          0 writes |
+----------------------------+------------------+-------------------+

ROOTS
+-----+-------------------------+-------------------------+
| IDX | ADDRESS                 | VALUE                   |
+-----+-------------------------+-------------------------+
|   1 | 00 00 7f ff f5 70 91 f0 | 00 00 51 10 00 00 01 a0 |
|   2 | 00 00 7f ff f5 70 92 10 | 00 00 51 10 00 00 01 c8 |
+-----+-------------------------+-------------------------+

BLOCKS
+-------------------------+-------------------------+-------------------------+
| FREELIST                | 00 00 51 10 00 00 01 f8 |                         |
+-------------------------+-------------------------+-------------------------+
| ADDRESS                 | VALUE                   | DESCRIPTION             |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 80 | 00 00 00 02 00 00 00 18 | size:         24   USED |
| 00 00 51 10 00 00 01 88 | 00 00 00 00 00 00 00 00 | field #1                |
| 00 00 51 10 00 00 01 90 | 00 00 00 00 00 00 00 00 | field #2                |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 98 | 00 00 00 02 00 00 00 18 | size:         24   USED |
| 00 00 51 10 00 00 01 a0 | 00 00 51 10 00 00 01 88 | field #1                |
| 00 00 51 10 00 00 01 a8 | 00 00 51 10 00 00 01 c8 | field #2                |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 b0 | 00 02 00 00 00 00 00 10 | size:         16   FREE |
| 00 00 51 10 00 00 01 b8 | 00 00 51 10 00 00 02 20 | next free block         |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 c0 | 00 00 00 02 00 00 00 18 | size:         24   USED |
| 00 00 51 10 00 00 01 c8 | 00 00 51 10 00 00 02 08 | field #1                |
| 00 00 51 10 00 00 01 d0 | 00 00 51 10 00 00 01 e0 | field #2                |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 d8 | 00 00 00 02 00 00 00 18 | size:         24   USED |
| 00 00 51 10 00 00 01 e0 | 00 00 00 00 00 00 00 00 | field #1                |
| 00 00 51 10 00 00 01 e8 | 00 00 00 00 00 00 00 00 | field #2                |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 01 f0 | 00 02 00 00 00 00 00 10 | size:         16   FREE |
| 00 00 51 10 00 00 01 f8 | 00 00 51 10 00 00 01 b8 | next free block         |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 02 00 | 00 00 00 02 00 00 00 18 | size:         24   USED |
| 00 00 51 10 00 00 02 08 | 00 00 00 00 00 00 00 00 | field #1                |
| 00 00 51 10 00 00 02 10 | 00 00 00 00 00 00 00 00 | field #2                |
+-------------------------+-------------------------+-------------------------+
| 00 00 51 10 00 00 02 18 | 00 02 00 00 00 00 00 68 | size:        104   FREE |
| 00 00 51 10 00 00 02 20 | 00 00 00 00 00 00 00 00 | next free block         |
| 00 00 51 10 00 00 02 28 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 30 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 38 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 40 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 48 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 50 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 58 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 60 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 68 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 70 | 00 00 00 00 00 00 00 00 |                         |
| 00 00 51 10 00 00 02 78 | 00 00 00 00 00 00 00 00 |                         |
+-------------------------+-------------------------+-------------------------+
```
