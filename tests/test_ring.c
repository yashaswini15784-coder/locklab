#include "locklab.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITEMS_PER_PRODUCER 1000
#define TOTAL_ITEMS (THREADS * ITEMS_PER_PRODUCER)

typedef struct {
    RingBuffer *ring;
    int producer;
    int start;
    _Atomic int *seen;
    _Atomic int *consumed;
} Worker;

static void *run_worker(void *arg) {
    Worker *worker = arg;

    if (worker->producer) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
            int value = worker->start + i;

            while (!ring_enqueue(worker->ring, value)) {
                sched_yield();
            }
        }
        return NULL;
    }

    while (atomic_load(worker->consumed) < TOTAL_ITEMS) {
        int value;

        if (ring_dequeue(worker->ring, &value)) {
            assert(value >= 0 && value < TOTAL_ITEMS);
            int previous = atomic_fetch_add(&worker->seen[value], 1);
            assert(previous == 0);
            atomic_fetch_add(worker->consumed, 1);
        } else {
            sched_yield();
        }
    }

    return NULL;
}

int main(void) {
    RingBuffer ring;
    pthread_t threads[THREADS * 2];
    Worker workers[THREADS * 2];

    _Atomic int *seen = malloc(sizeof(*seen) * TOTAL_ITEMS);
    assert(seen != NULL);

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        atomic_init(&seen[i], 0);
    }

    _Atomic int consumed;
    atomic_init(&consumed, 0);

    assert(ring_init(&ring, 1024) == 0);

    for (int i = 0; i < THREADS * 2; i++) {
        workers[i].ring = &ring;
        workers[i].seen = seen;
        workers[i].consumed = &consumed;

        if (i < THREADS) {
            workers[i].producer = 1;
            workers[i].start = i * ITEMS_PER_PRODUCER;
        } else {
            workers[i].producer = 0;
            workers[i].start = 0;
        }

        assert(pthread_create(&threads[i], NULL, run_worker,
                              &workers[i]) == 0);
    }

    for (int i = 0; i < THREADS * 2; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    assert(atomic_load(&consumed) == TOTAL_ITEMS);

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        assert(atomic_load(&seen[i]) == 1);
    }

    int value;
    assert(ring_dequeue(&ring, &value) == 0);

    ring_destroy(&ring);
    free(seen);

    printf("MPMC ring test passed: %d items\n", TOTAL_ITEMS);
    return 0;
}
