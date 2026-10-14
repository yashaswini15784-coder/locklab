#include "locklab.h"
#include <stdlib.h>

struct LockFreeStackNode {
    int value;
    struct LockFreeStackNode *next;
};

void lf_stack_init(LockFreeStack *stack) {
    atomic_init(&stack->head, NULL);
}

void lf_stack_push(LockFreeStack *stack, int value) {
    LockFreeStackNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        abort();
    }

    node->value = value;

    LockFreeStackNode *old_head =
        atomic_load_explicit(&stack->head, memory_order_relaxed);

    do {
        node->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(
        &stack->head,
        &old_head,
        node,
        memory_order_release,
        memory_order_relaxed
    ));
}

int lf_stack_pop(
    LockFreeStack *stack,
    HazardParticipant *participant,
    int *value
) {
    for (;;) {
        LockFreeStackNode *head =
            atomic_load_explicit(&stack->head, memory_order_seq_cst);

        if (head == NULL) {
            hazard_clear(participant);
            return 0;
        }

        atomic_store_explicit(
            &participant->domain->slots[participant->id][0],
            head,
            memory_order_seq_cst
        );

        if (head != atomic_load_explicit(&stack->head, memory_order_seq_cst)) {
            continue;
        }

        LockFreeStackNode *next = head->next;

        if (atomic_compare_exchange_weak_explicit(
                &stack->head,
                &head,
                next,
                memory_order_seq_cst,
                memory_order_seq_cst
            )) {
            *value = head->value;

            hazard_clear(participant);
            hazard_retire(participant, head);
            return 1;
        }
    }
}

void lf_stack_destroy(LockFreeStack *stack) {
    LockFreeStackNode *node =
        atomic_load_explicit(&stack->head, memory_order_relaxed);

    while (node != NULL) {
        LockFreeStackNode *next = node->next;
        free(node);
        node = next;
    }

    atomic_store_explicit(&stack->head, NULL, memory_order_relaxed);
}
