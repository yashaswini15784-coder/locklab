#include "locklab.h"
#include <stdlib.h>

struct LockFreeQueueNode {
    int value;
    _Atomic(struct LockFreeQueueNode *) next;
};

static LockFreeQueueNode *protect_pointer(
    _Atomic(LockFreeQueueNode *) *source,
    HazardParticipant *participant,
    unsigned slot
) {
    LockFreeQueueNode *pointer;

    do {
        pointer = atomic_load_explicit(source, memory_order_seq_cst);
        atomic_store_explicit(
            &participant->domain->slots[participant->id][slot],
            pointer,
            memory_order_seq_cst
        );
    } while (pointer != atomic_load_explicit(source, memory_order_seq_cst));

    return pointer;
}

static void clear_queue_hazards(HazardParticipant *participant) {
    for (unsigned slot = 0; slot < 3; slot++) {
        atomic_store_explicit(
            &participant->domain->slots[participant->id][slot],
            NULL,
            memory_order_seq_cst
        );
    }
}

void lf_queue_init(LockFreeQueue *queue) {
    LockFreeQueueNode *dummy = malloc(sizeof(*dummy));
    if (dummy == NULL) {
        abort();
    }

    dummy->value = 0;
    atomic_init(&dummy->next, NULL);
    atomic_init(&queue->head, dummy);
    atomic_init(&queue->tail, dummy);
}

void lf_queue_enqueue(
    LockFreeQueue *queue,
    HazardParticipant *participant,
    int value
) {
    LockFreeQueueNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        abort();
    }

    node->value = value;
    atomic_init(&node->next, NULL);

    for (;;) {
        LockFreeQueueNode *tail =
            protect_pointer(&queue->tail, participant, 2);

        LockFreeQueueNode *next =
            atomic_load_explicit(&tail->next, memory_order_seq_cst);

        atomic_store_explicit(
            &participant->domain->slots[participant->id][1],
            next,
            memory_order_seq_cst
        );

        if (tail != atomic_load_explicit(&queue->tail, memory_order_seq_cst) ||
            next != atomic_load_explicit(&tail->next, memory_order_seq_cst)) {
            continue;
        }

        if (next == NULL) {
            LockFreeQueueNode *expected = NULL;

            if (atomic_compare_exchange_weak_explicit(
                    &tail->next,
                    &expected,
                    node,
                    memory_order_seq_cst,
                    memory_order_seq_cst
                )) {
                atomic_compare_exchange_strong_explicit(
                    &queue->tail,
                    &tail,
                    node,
                    memory_order_seq_cst,
                    memory_order_seq_cst
                );

                clear_queue_hazards(participant);
                return;
            }
        } else {
            atomic_compare_exchange_weak_explicit(
                &queue->tail,
                &tail,
                next,
                memory_order_seq_cst,
                memory_order_seq_cst
            );
        }
    }
}

int lf_queue_dequeue(
    LockFreeQueue *queue,
    HazardParticipant *participant,
    int *value
) {
    for (;;) {
        LockFreeQueueNode *head =
            protect_pointer(&queue->head, participant, 0);

        LockFreeQueueNode *tail =
            protect_pointer(&queue->tail, participant, 2);

        LockFreeQueueNode *next =
            atomic_load_explicit(&head->next, memory_order_seq_cst);

        atomic_store_explicit(
            &participant->domain->slots[participant->id][1],
            next,
            memory_order_seq_cst
        );

        if (head != atomic_load_explicit(&queue->head, memory_order_seq_cst) ||
            next != atomic_load_explicit(&head->next, memory_order_seq_cst)) {
            continue;
        }

        if (head == tail) {
            if (next == NULL) {
                clear_queue_hazards(participant);
                return 0;
            }

            atomic_compare_exchange_weak_explicit(
                &queue->tail,
                &tail,
                next,
                memory_order_seq_cst,
                memory_order_seq_cst
            );
            continue;
        }

        if (next == NULL) {
            continue;
        }

        int result = next->value;

        if (atomic_compare_exchange_weak_explicit(
                &queue->head,
                &head,
                next,
                memory_order_seq_cst,
                memory_order_seq_cst
            )) {
            *value = result;
            clear_queue_hazards(participant);
            hazard_retire(participant, head);
            return 1;
        }
    }
}

void lf_queue_destroy(LockFreeQueue *queue) {
    LockFreeQueueNode *node =
        atomic_load_explicit(&queue->head, memory_order_relaxed);

    while (node != NULL) {
        LockFreeQueueNode *next =
            atomic_load_explicit(&node->next, memory_order_relaxed);
        free(node);
        node = next;
    }

    atomic_store_explicit(&queue->head, NULL, memory_order_relaxed);
    atomic_store_explicit(&queue->tail, NULL, memory_order_relaxed);
}
