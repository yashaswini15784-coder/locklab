#define _POSIX_C_SOURCE 200809L
#include "locklab.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define ITERATIONS 100000
#define MAX_THREADS 32

typedef struct {
    int lock_free;
    Stack *locked_stack;
    LockFreeStack *lf_stack;
    HazardParticipant *participant;
} Worker;

static void *run_operations(void *arg) {
    Worker *worker = arg;

    for (int i = 0; i < ITERATIONS; i++) {
        int value;

        if (worker->lock_free) {
            lf_stack_push(worker->lf_stack, i);
            assert(lf_stack_pop(worker->lf_stack,
                               worker->participant, &value) == 1);
        } else {
            assert(stack_push(worker->locked_stack, i) == 0);
            assert(stack_pop(worker->locked_stack, &value) == 1);
        }
    }

    return NULL;
}

static double seconds_between(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static void benchmark(int lock_free, int thread_count) {
    Stack locked_stack;
    LockFreeStack lf_stack;
    HazardDomain domain;
    HazardParticipant participants[MAX_THREADS];
    pthread_t threads[MAX_THREADS];
    Worker workers[MAX_THREADS];

    if (lock_free) {
        hazard_domain_init(&domain);
        lf_stack_init(&lf_stack);

        for (int i = 0; i < thread_count; i++) {
            assert(hazard_participant_init(&domain, (unsigned)i,
                                           &participants[i]) == 0);
        }
    } else {
        assert(stack_init(&locked_stack) == 0);
    }

    for (int i = 0; i < thread_count; i++) {
        workers[i].lock_free = lock_free;
        workers[i].locked_stack = &locked_stack;
        workers[i].lf_stack = &lf_stack;
        workers[i].participant = lock_free ? &participants[i] : NULL;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < thread_count; i++) {
        assert(pthread_create(&threads[i], NULL, run_operations,
                              &workers[i]) == 0);
    }

    for (int i = 0; i < thread_count; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = seconds_between(start, end);
    double operations = 2.0 * ITERATIONS * thread_count;
    printf("%-10s threads=%d time=%.3f s throughput=%.0f ops/s\n",
           lock_free ? "lock-free" : "mutex",
           thread_count, elapsed, operations / elapsed);

    if (lock_free) {
        for (int i = 0; i < thread_count; i++) {
            hazard_flush(&participants[i]);
        }
        lf_stack_destroy(&lf_stack);
    } else {
        stack_destroy(&locked_stack);
    }
}

int main(void) {
    int thread_counts[] = {1, 2, 4, 8, 12, 16, 24, 32};

    for (unsigned i = 0; i < sizeof(thread_counts) / sizeof(thread_counts[0]);
         i++) {
        int count = thread_counts[i];
        benchmark(0, count);
        benchmark(1, count);
    }

    return 0;
}
