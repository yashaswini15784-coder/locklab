# LockLab

Lock-free data structures in C11: a Treiber stack, Michael–Scott queue, hazard-pointer reclamation, an ABA demonstration, a bounded MPMC ring buffer, stress tests, and a mutex-versus-lock-free benchmark.

## Requirements

- GCC with C11 atomics
- CMake
- pthreads

Install the build tools on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/test_stack
./build/test_lockfree_stack
./build/test_lockfree_queue
./build/test_ring
./build/aba_demo
./build/benchmark
```

## Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cmake -S . -B build-asan -DCMAKE_C_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan
./build-asan/test_lockfree_stack
./build-asan/test_lockfree_queue
./build-asan/test_ring
```

ThreadSanitizer:

```bash
cmake -S . -B build-tsan -DCMAKE_C_FLAGS="-O1 -g -fsanitize=thread"
cmake --build build-tsan
./build-tsan/test_lockfree_stack
./build-tsan/test_lockfree_queue
./build-tsan/test_ring
```

## Benchmark

```bash
./build/benchmark
```

The benchmark compares paired push/pop operations at 1, 2, 4, 8, 12, 16, 24, and 32 threads. In the latest run on a 12-logical-CPU machine, lock-free throughput was about 32.00M ops/s at 1 thread and 17.02M ops/s at 2 threads, versus 30.29M and 16.38M ops/s for the mutex stack. At 4 threads and above, the mutex stack was faster in that run. Results varied between runs.

See `docs/design.md` for algorithm, reclamation, ABA, and memory-ordering details.
