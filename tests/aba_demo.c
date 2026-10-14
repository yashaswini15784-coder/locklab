#include <stdatomic.h>
#include <stdio.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

int main(void) {
    Node a = { .value = 1, .next = NULL };
    Node b = { .value = 2, .next = NULL };
    Node c = { .value = 3, .next = NULL };

    a.next = &b;
    b.next = &c;

    _Atomic(Node *) head;
    atomic_init(&head, &a);

    /* Thread 1 reads head and next, then pauses. */
    Node *thread1_head = atomic_load(&head);
    Node *thread1_next = thread1_head->next;

    /* Thread 2 pops A, then B. */
    atomic_store(&head, &b);
    atomic_store(&head, &c);

    /* Thread 2 pushes the same A address back. */
    a.next = atomic_load(&head);
    atomic_store(&head, &a);

    /* Thread 1's stale CAS sees A again and incorrectly succeeds. */
    Node *expected = thread1_head;
    int cas_succeeded = atomic_compare_exchange_strong(&head, &expected,
                                                        thread1_next);

    printf("Thread 1 stale CAS: %s\n",
           cas_succeeded ? "succeeded" : "failed");

    Node *final_head = atomic_load(&head);
    printf("Final head value: %d\n", final_head->value);
    printf("Current stack should start at A (value 1), but A was lost: %s\n",
           final_head == &b ? "yes" : "no");

    return !(cas_succeeded && final_head == &b);
}
