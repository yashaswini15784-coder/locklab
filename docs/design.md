# LockLab Design

## Data structures

### Mutex baseline stack

`Stack` is a linked stack protected by a pthread mutex. It provides the baseline for the stack benchmark.

### Treiber stack

`LockFreeStack` stores its head in an atomic pointer. Push initializes a node and publishes it with compare-and-swap (CAS). Pop publishes the observed head in a hazard pointer, validates that the head has not changed, reads the next pointer, and then attempts to advance the head with CAS.

### Michael–Scott queue

`LockFreeQueue` uses a dummy node and atomic head and tail pointers. Enqueue links a new node with CAS and helps advance the tail. Dequeue protects the head, next, and tail pointers with hazard slots before attempting to advance the head. The old dummy node is retired for later reclamation.

### Bounded MPMC ring buffer

`RingBuffer` is a fixed-capacity, power-of-two array. Each cell has a sequence counter. Producers and consumers reserve positions with CAS, then use the cell sequence to publish or release that slot. Enqueue returns `0` when full; dequeue returns `0` when empty.

## Hazard-pointer reclamation

Register one hazard participant per worker before starting worker threads. A participant must be used by only one thread at a time.

Removed nodes are added to that participant's retired list. When the list reaches its scan threshold, the participant scans registered hazard slots and frees only retired nodes that are not protected. After workers stop and clear their hazards, call `hazard_flush` for every participant to reclaim remaining retired nodes.

## ABA problem

`aba_demo` models this naive-stack interleaving:

1. Thread 1 reads head A and remembers successor B.
2. Thread 2 removes A and B, then makes A's address the head again.
3. Thread 1's stale CAS sees address A again and succeeds, replacing the head with stale B.

The current A node is lost from the stack.

The Treiber stack publishes the observed head in a hazard pointer and validates the head before dereferencing it. A node protected by a hazard pointer cannot be reclaimed and reused during that operation, preventing the stale-address reuse shown in the naive demonstration.

## Memory ordering

The implementation uses stronger ordering in some places to keep the correctness argument clear:

- Treiber push uses a release CAS to publish the initialized node. Failed CAS reads are relaxed because they only refresh the expected head for a retry.
- Treiber pop uses sequentially consistent head observations and CAS, and sequentially consistent hazard publication and scans. It validates the head after publishing the hazard and before dereferencing the node.
- Michael–Scott queue pointer operations use sequentially consistent ordering, giving head, tail, link, and hazard validation operations one clear total order.
- The ring buffer publishes cell data with a release store to the sequence counter. A consumer uses an acquire load before reading that data. The consumer releases a cell with a release store; a producer uses an acquire load before reusing it. Position-reservation CAS operations are relaxed because the cell sequence counter synchronizes data handoff.

Some queue and hazard-pointer operations may be candidates for weaker acquire/release ordering. Any such change requires a separate ordering proof and renewed stress and sanitizer runs.

## Validation

The stack and queue tests use four producer threads to add 4,000 unique values, then four consumer threads to remove them. They verify the count and that every value is observed exactly once.

The ring-buffer test runs four producers and four consumers concurrently with 4,000 values, checking for missing or duplicate values.

The latest implementation passed the stack, queue, and ring tests in AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer builds. `aba_demo` reports that the stale CAS succeeds and the head becomes B.

## BenchMark
In the latest run on a 12-logical-CPU machine, lock-free throughput was about 32.00M ops/s at 1 thread and 17.02M ops/s at 2 threads, versus 30.29M and 16.38M ops/s for the mutex stack. At 4 threads and above, the mutex stack was faster in that run. Timings varied between runs; repeat measurements and report the machine and workload context rather than assuming lock-free always wins.
