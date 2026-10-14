#ifndef LOCKLAB_H
#define LOCKLAB_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

#define LL_MAX_THREADS 64
#define LL_HAZARD_SLOTS 3
#define LL_RETIRED_LIMIT 32

/* Mutex-based baseline stack */
typedef struct StackNode {
    int value;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode *top;
    pthread_mutex_t mutex;
} Stack;

int stack_init(Stack *stack);
void stack_destroy(Stack *stack);
int stack_push(Stack *stack, int value);
int stack_pop(Stack *stack, int *value);

/* Hazard-pointer domain */
typedef struct {
    _Atomic(void *) slots[LL_MAX_THREADS][LL_HAZARD_SLOTS];
    _Atomic unsigned active_count;
} HazardDomain;

typedef struct {
    HazardDomain *domain;
    unsigned id;
    void *retired[LL_RETIRED_LIMIT];
    size_t retired_count;
} HazardParticipant;

void hazard_domain_init(HazardDomain *domain);
int hazard_participant_init(
    HazardDomain *domain,
    unsigned id,
    HazardParticipant *participant
);
void hazard_clear(HazardParticipant *participant);
void hazard_retire(HazardParticipant *participant, void *pointer);
void hazard_flush(HazardParticipant *participant);

/* Lock-free Treiber stack */
typedef struct LockFreeStackNode LockFreeStackNode;

typedef struct {
    _Atomic(LockFreeStackNode *) head;
} LockFreeStack;

void lf_stack_init(LockFreeStack *stack);
void lf_stack_push(LockFreeStack *stack, int value);
int lf_stack_pop(
    LockFreeStack *stack,
    HazardParticipant *participant,
    int *value
);
void lf_stack_destroy(LockFreeStack *stack);
/* Michael-Scott lock-free queue */
typedef struct LockFreeQueueNode LockFreeQueueNode;

typedef struct {
    _Atomic(LockFreeQueueNode *) head;
    _Atomic(LockFreeQueueNode *) tail;
} LockFreeQueue;

void lf_queue_init(LockFreeQueue *queue);
void lf_queue_enqueue(
    LockFreeQueue *queue,
    HazardParticipant *participant,
    int value
);
int lf_queue_dequeue(
    LockFreeQueue *queue,
    HazardParticipant *participant,
    int *value
);
void lf_queue_destroy(LockFreeQueue *queue);
/* Bounded lock-free MPMC ring buffer */
typedef struct RingCell RingCell;

typedef struct {
    RingCell *cells;
    size_t capacity;
    size_t mask;
    _Atomic size_t enqueue_pos;
    _Atomic size_t dequeue_pos;
} RingBuffer;

int ring_init(RingBuffer *ring, size_t capacity);
void ring_destroy(RingBuffer *ring);
int ring_enqueue(RingBuffer *ring, int value);
int ring_dequeue(RingBuffer *ring, int *value);
#endif
