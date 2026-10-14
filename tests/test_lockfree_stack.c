#include "locklab.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define MAX_ITEMS 100000

typedef struct {
    LockFreeStack *stack;
    HazardParticipant *participant;
    int start;
    int count;
    int total_items;
    _Atomic int *seen;
    _Atomic int *popped;
} Worker;

static void *push_items(void *arg) {
    Worker *worker = arg;

    for (int i = 0; i < worker->count; i++)
        lf_stack_push(worker->stack, worker->start + i);

    return NULL;
}

static void *pop_items(void *arg) {
    Worker *worker = arg;
    int value;

    while (atomic_load(worker->popped) < worker->total_items) {
        if (lf_stack_pop(worker->stack, worker->participant, &value)) {
            assert(value >= 0 && value < worker->total_items);
            int previous = atomic_fetch_add(&worker->seen[value], 1);
            assert(previous == 0);
            atomic_fetch_add(worker->popped, 1);
        } else {
            sched_yield();
        }
    }

    hazard_clear(worker->participant);
    return NULL;
}

int main(void) {
    int total_items;

    printf("How many values should the stack test process? (1-%d): ", MAX_ITEMS);
    if (scanf("%d", &total_items) != 1 ||
        total_items < 1 || total_items > MAX_ITEMS) {
        puts("Invalid count.");
        return 1;
    }

    LockFreeStack stack;
    HazardDomain domain;
    pthread_t threads[THREADS];
    Worker workers[THREADS];
    HazardParticipant participants[THREADS * 2];

    _Atomic int *seen = malloc(sizeof(*seen) * (size_t)total_items);
    assert(seen != NULL);

    for (int i = 0; i < total_items; i++)
        atomic_init(&seen[i], 0);

    _Atomic int popped;
    atomic_init(&popped, 0);

    hazard_domain_init(&domain);
    lf_stack_init(&stack);

    int start = 0;
    for (int i = 0; i < THREADS; i++) {
        int count = total_items / THREADS + (i < total_items % THREADS);

        assert(hazard_participant_init(&domain, (unsigned)i,
                                       &participants[i]) == 0);
        workers[i].stack = &stack;
        workers[i].participant = &participants[i];
        workers[i].start = start;
        workers[i].count = count;
        workers[i].total_items = total_items;
        workers[i].seen = seen;
        workers[i].popped = &popped;
        start += count;

        assert(pthread_create(&threads[i], NULL, push_items, &workers[i]) == 0);
    }

    for (int i = 0; i < THREADS; i++)
        assert(pthread_join(threads[i], NULL) == 0);

    for (int i = 0; i < THREADS; i++) {
        unsigned id = (unsigned)(THREADS + i);
        assert(hazard_participant_init(&domain, id, &participants[id]) == 0);
        workers[i].participant = &participants[id];
        assert(pthread_create(&threads[i], NULL, pop_items, &workers[i]) == 0);
    }

    for (int i = 0; i < THREADS; i++)
        assert(pthread_join(threads[i], NULL) == 0);

    for (int i = 0; i < THREADS * 2; i++)
        hazard_flush(&participants[i]);

    assert(atomic_load(&popped) == total_items);
    for (int i = 0; i < total_items; i++)
        assert(atomic_load(&seen[i]) == 1);

    lf_stack_destroy(&stack);
    free(seen);

    printf("Producer threads: %d\n", THREADS);
    printf("Consumer threads: %d\n", THREADS);
    printf("Requested items: %d\n", total_items);
    printf("Successfully processed: %d\n", atomic_load(&popped));
    puts("Missing or duplicate values: 0");
    puts("Lock-free stack test: PASS");
    return 0;
}

