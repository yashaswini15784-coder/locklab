#include "locklab.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREAD_COUNT 4
#define ITEMS_PER_THREAD 1000

typedef struct {
    Stack *stack;
    int start;
} WorkerData;

void *push_items(void *arg) {
    WorkerData *data = arg;

    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        int value = data->start + i;
        assert(stack_push(data->stack, value) == 0);
    }

    return NULL;
}

int main(void) {
    Stack stack;
    pthread_t threads[THREAD_COUNT];
    WorkerData data[THREAD_COUNT];
    int total = THREAD_COUNT * ITEMS_PER_THREAD;

    assert(stack_init(&stack) == 0);

    for (int i = 0; i < THREAD_COUNT; i++) {
        data[i].stack = &stack;
        data[i].start = i * ITEMS_PER_THREAD;
        assert(pthread_create(&threads[i], NULL, push_items, &data[i]) == 0);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }

    int *seen = calloc((size_t)total, sizeof(int));
    assert(seen != NULL);

    int value;
    int popped = 0;

    while (stack_pop(&stack, &value)) {
        assert(value >= 0 && value < total);
        assert(seen[value] == 0);
        seen[value] = 1;
        popped++;
    }

    assert(popped == total);

    free(seen);
    stack_destroy(&stack);
    printf("Multi-thread stack test passed: %d items\n", popped);
    return 0;
}
