#include "locklab.h"
#include <stdlib.h>

int stack_init(Stack *stack) {
    stack->top = NULL;
    return pthread_mutex_init(&stack->mutex, NULL);
}

void stack_destroy(Stack *stack) {
    pthread_mutex_lock(&stack->mutex);

    StackNode *node = stack->top;
    while (node != NULL) {
        StackNode *next = node->next;
        free(node);
        node = next;
    }

    stack->top = NULL;
    pthread_mutex_unlock(&stack->mutex);
    pthread_mutex_destroy(&stack->mutex);
}

int stack_push(Stack *stack, int value) {
    StackNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->value = value;

    pthread_mutex_lock(&stack->mutex);
    node->next = stack->top;
    stack->top = node;
    pthread_mutex_unlock(&stack->mutex);

    return 0;
}

int stack_pop(Stack *stack, int *value) {
    pthread_mutex_lock(&stack->mutex);

    StackNode *node = stack->top;
    if (node == NULL) {
        pthread_mutex_unlock(&stack->mutex);
        return 0;
    }

    stack->top = node->next;
    *value = node->value;
    pthread_mutex_unlock(&stack->mutex);

    free(node);
    return 1;
}
