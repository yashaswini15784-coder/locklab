#include "locklab.h"
#include <stdlib.h>

void hazard_domain_init(HazardDomain *domain) {
    for (unsigned i = 0; i < LL_MAX_THREADS; i++) {
        for (unsigned j = 0; j < LL_HAZARD_SLOTS; j++) {
            atomic_init(&domain->slots[i][j], NULL);
        }
    }

    atomic_init(&domain->active_count, 0);
}

int hazard_participant_init(
    HazardDomain *domain,
    unsigned id,
    HazardParticipant *participant
) {
    if (id >= LL_MAX_THREADS) {
        return -1;
    }

    participant->domain = domain;
    participant->id = id;
    participant->retired_count = 0;

    unsigned wanted = id + 1;
    unsigned current = atomic_load_explicit(
        &domain->active_count,
        memory_order_relaxed
    );

    while (current < wanted &&
           !atomic_compare_exchange_weak_explicit(
               &domain->active_count,
               &current,
               wanted,
               memory_order_release,
               memory_order_relaxed
           )) {
        /* CAS updates current; retry if a larger id registered meanwhile. */
    }

    return 0;
}

void hazard_clear(HazardParticipant *participant) {
    atomic_store_explicit(
        &participant->domain->slots[participant->id][0],
        NULL,
        memory_order_seq_cst
    );
}

static int pointer_is_hazard(HazardDomain *domain, void *pointer) {
    unsigned count = atomic_load_explicit(
        &domain->active_count,
        memory_order_acquire
    );

    for (unsigned i = 0; i < count; i++) {
        for (unsigned j = 0; j < LL_HAZARD_SLOTS; j++) {
            void *hazard = atomic_load_explicit(
                &domain->slots[i][j],
                memory_order_seq_cst
            );

            if (hazard == pointer) {
                return 1;
            }
        }
    }

    return 0;
}

static void hazard_scan(HazardParticipant *participant) {
    size_t keep = 0;

    for (size_t i = 0; i < participant->retired_count; i++) {
        void *pointer = participant->retired[i];

        if (pointer_is_hazard(participant->domain, pointer)) {
            participant->retired[keep++] = pointer;
        } else {
            free(pointer);
        }
    }

    participant->retired_count = keep;
}

void hazard_retire(HazardParticipant *participant, void *pointer) {
    participant->retired[participant->retired_count++] = pointer;

    if (participant->retired_count == LL_RETIRED_LIMIT) {
        hazard_scan(participant);
    }
}

void hazard_flush(HazardParticipant *participant) {
    hazard_scan(participant);
}
