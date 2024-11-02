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
