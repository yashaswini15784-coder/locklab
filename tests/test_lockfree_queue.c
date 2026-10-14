#include "locklab.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITEMS_PER_THREAD 1000
#define TOTAL_ITEMS (THREADS * ITEMS_PER_THREAD)

typedef struct {
    LockFreeQueue *queue;
    HazardParticipant *participant;
    int start;
    _Atomic int *seen;
    _Atomic int *dequeued;
} Worker;

static void *enqueue_items(void *arg) {
    Worker *worker = arg;

    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        lf_queue_enqueue(worker->queue, worker->participant,
                         worker->start + i);
    }

    hazard_clear(worker->participant);
    return NULL;
}

static void *dequeue_items(void *arg) {
    Worker *worker = arg;
    int value;

    while (atomic_load(worker->dequeued) < TOTAL_ITEMS) {
        if (lf_queue_dequeue(worker->queue, worker->participant, &value)) {
            assert(value >= 0 && value < TOTAL_ITEMS);

            int previous = atomic_fetch_add(&worker->seen[value], 1);
            assert(previous == 0);

            atomic_fetch_add(worker->dequeued, 1);
        } else {
            sched_yield();
        }
    }

    hazard_clear(worker->participant);
    return NULL;
}

int main(void) {
    LockFreeQueue queue;
    HazardDomain domain;
    pthread_t threads[THREADS];
    Worker workers[THREADS];
    HazardParticipant participants[THREADS * 2];

    _Atomic int *seen = malloc(sizeof(*seen) * TOTAL_ITEMS);
    assert(seen != NULL);

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        atomic_init(&seen[i], 0);
    }

    _Atomic int dequeued;
    atomic_init(&dequeued, 0);

    hazard_domain_init(&domain);
    lf_queue_init(&queue);

    for (int i = 0; i < THREADS; i++) {
        assert(hazard_participant_init(&domain, (unsigned)i,
                                       &participants[i]) == 0);
        workers[i].queue = &queue;
        workers[i].participant = &participants[i];
        workers[i].start = i * ITEMS_PER_THREAD;
        workers[i].seen = seen;
        workers[i].dequeued = &dequeued;

        assert(pthread_create(&threads[i], NULL, enqueue_items,
                              &workers[i]) == 0);
    }

    for (int i = 0; i < THREADS; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    for (int i = 0; i < THREADS; i++) {
        unsigned id = (unsigned)(THREADS + i);
        assert(hazard_participant_init(&domain, id, &participants[id]) == 0);
        workers[i].participant = &participants[id];

        assert(pthread_create(&threads[i], NULL, dequeue_items,
                              &workers[i]) == 0);
    }

    for (int i = 0; i < THREADS; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    for (int i = 0; i < THREADS * 2; i++) {
        hazard_flush(&participants[i]);
    }

    assert(atomic_load(&dequeued) == TOTAL_ITEMS);

    for (int i = 0; i < TOTAL_ITEMS; i++) {
        assert(atomic_load(&seen[i]) == 1);
    }

    lf_queue_destroy(&queue);
    free(seen);

    printf("Lock-free queue test passed: %d items\n", TOTAL_ITEMS);
    return 0;
}
