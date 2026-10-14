#include "locklab.h"
#include <stdio.h>

#define MAX_VALUES 10

static void run_stack(HazardDomain *domain, int *values, int count) {
    HazardParticipant participant;
    LockFreeStack stack;
    int passed = 1;

    hazard_participant_init(domain, 0, &participant);
    lf_stack_init(&stack);

    puts("\n--- Treiber lock-free stack ---");
    puts("Stack = last in, first out (LIFO)");

    for (int i = 0; i < count; i++) {
        lf_stack_push(&stack, values[i]);
        printf("push(%d): added to stack\n", values[i]);
    }

    puts("Popping values:");
    for (int i = count - 1; i >= 0; i--) {
        int value;
        if (!lf_stack_pop(&stack, &participant, &value)) {
            passed = 0;
            break;
        }
        printf("pop() -> %d\n", value);
        if (value != values[i]) passed = 0;
    }

    hazard_clear(&participant);
    hazard_flush(&participant);
    lf_stack_destroy(&stack);
    printf("Stack check: %s\n", passed ? "PASS" : "FAIL");
}

static void run_queue(HazardDomain *domain, int *values, int count) {
    HazardParticipant participant;
    LockFreeQueue queue;
    int passed = 1;

    hazard_participant_init(domain, 1, &participant);
    lf_queue_init(&queue);

    puts("\n--- Michael-Scott lock-free queue ---");
    puts("Queue = first in, first out (FIFO)");

    for (int i = 0; i < count; i++) {
        lf_queue_enqueue(&queue, &participant, values[i]);
        printf("enqueue(%d): added to queue\n", values[i]);
    }

    puts("Dequeuing values:");
    for (int i = 0; i < count; i++) {
        int value;
        if (!lf_queue_dequeue(&queue, &participant, &value)) {
            passed = 0;
            break;
        }
        printf("dequeue() -> %d\n", value);
        if (value != values[i]) passed = 0;
    }

    hazard_clear(&participant);
    hazard_flush(&participant);
    lf_queue_destroy(&queue);
    printf("Queue check: %s\n", passed ? "PASS" : "FAIL");
}

static void run_aba_demo(void) {
    int a = 1, b = 2, c = 3;
    int *head = &a;

    int *thread1_old_head = head;
    int *thread1_old_next = &b;

    /* Thread 2 removes A, then B, and puts A's address back. */
    head = &c;
    head = &a;

    puts("\n--- ABA problem demonstration ---");
    puts("Thread 1 reads head A and remembers next B.");
    puts("Thread 2 changes the head, then puts A's address back.");
    if (head == thread1_old_head) {
        head = thread1_old_next;
        printf("Thread 1 stale CAS: succeeded\n");
        printf("Head now points to value %d; current A node was lost: yes\n", *head);
        puts("Hazard pointers help prevent a removed node from being freed and reused while another thread examines it.");
    } else {
        puts("Stale CAS did not succeed.");
    }

    (void)c;
}

int main(void) {
    int choice;
    int values[MAX_VALUES];
    int count = 0;

    puts("====================================");
    puts("       LockLab Interactive Demo");
    puts("====================================");
    puts("1 - Full demo: stack + queue + ABA");
    puts("2 - Stack demo");
    puts("3 - Queue demo");
    puts("4 - ABA demo");
    puts("0 - Exit");
    printf("Select option: ");

    if (scanf("%d", &choice) != 1 || choice < 0 || choice > 4) {
        puts("Invalid option.");
        return 1;
    }

    if (choice == 0) {
        puts("Demo closed.");
        return 0;
    }

    if (choice != 4) {
        printf("How many values? (1-%d): ", MAX_VALUES);
        if (scanf("%d", &count) != 1 || count < 1 || count > MAX_VALUES) {
            puts("Enter a count from 1 to 10.");
            return 1;
        }

        printf("Enter %d integer values, separated by spaces: ", count);
        for (int i = 0; i < count; i++) {
            if (scanf("%d", &values[i]) != 1) {
                puts("Please enter integer values.");
                return 1;
            }
        }
    }

    HazardDomain domain;
    hazard_domain_init(&domain);

    if (choice == 1 || choice == 2)
        run_stack(&domain, values, count);

    if (choice == 1 || choice == 3)
        run_queue(&domain, values, count);

    if (choice == 1 || choice == 4)
        run_aba_demo();

    puts("\nDemo finished.");
    return 0;
}
