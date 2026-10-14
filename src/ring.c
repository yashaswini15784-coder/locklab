#include "locklab.h"

#include <stdint.h>
#include <stdlib.h>

struct RingCell {
    _Atomic size_t sequence;
    int value;
};

int ring_init(RingBuffer *ring, size_t capacity) {
    if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
        return -1;
    }

    ring->cells = calloc(capacity, sizeof(*ring->cells));
    if (ring->cells == NULL) {
        return -1;
    }

    ring->capacity = capacity;
    ring->mask = capacity - 1;
    atomic_init(&ring->enqueue_pos, 0);
    atomic_init(&ring->dequeue_pos, 0);

    for (size_t i = 0; i < capacity; i++) {
        atomic_init(&ring->cells[i].sequence, i);
    }

    return 0;
}

void ring_destroy(RingBuffer *ring) {
    free(ring->cells);
    ring->cells = NULL;
    ring->capacity = 0;
    ring->mask = 0;
}

int ring_enqueue(RingBuffer *ring, int value) {
    size_t position =
        atomic_load_explicit(&ring->enqueue_pos, memory_order_relaxed);
    RingCell *cell;

    for (;;) {
        cell = &ring->cells[position & ring->mask];

        size_t sequence =
            atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t difference =
            (intptr_t)sequence - (intptr_t)position;

        if (difference == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &ring->enqueue_pos,
                    &position,
                    position + 1,
                    memory_order_relaxed,
                    memory_order_relaxed
                )) {
                break;
            }
        } else if (difference < 0) {
            return 0; /* Buffer full. */
        } else {
            position =
                atomic_load_explicit(&ring->enqueue_pos, memory_order_relaxed);
        }
    }

    cell->value = value;
    atomic_store_explicit(
        &cell->sequence,
        position + 1,
        memory_order_release
    );
    return 1;
}

int ring_dequeue(RingBuffer *ring, int *value) {
    size_t position =
        atomic_load_explicit(&ring->dequeue_pos, memory_order_relaxed);
    RingCell *cell;

    for (;;) {
        cell = &ring->cells[position & ring->mask];

        size_t sequence =
            atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t difference =
            (intptr_t)sequence - (intptr_t)(position + 1);

        if (difference == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &ring->dequeue_pos,
                    &position,
                    position + 1,
                    memory_order_relaxed,
                    memory_order_relaxed
                )) {
                break;
            }
        } else if (difference < 0) {
            return 0; /* Buffer empty. */
        } else {
            position =
                atomic_load_explicit(&ring->dequeue_pos, memory_order_relaxed);
        }
    }

    *value = cell->value;
    atomic_store_explicit(
        &cell->sequence,
        position + ring->capacity,
        memory_order_release
    );
    return 1;
}
