#include "sync.h"

int sync_init(SyncState *state) {
    if (state == nullptr) return -1;

    state->active_readers  = 0;
    state->active_writers  = 0;
    state->waiting_writers = 0;
    state->next_ticket     = 0;
    state->turn_number     = 0;

    for (int i = 0; i < ROLE_COUNT; i++)
        for (int j = 0; j < ACTION_COUNT; j++)
            state->permissions[i][j] = 0;

    // Default permission matrix
    state->permissions[ROLE_USER][ACTION_READ]     = 1;
    state->permissions[ROLE_EDITOR][ACTION_READ]   = 1;
    state->permissions[ROLE_EDITOR][ACTION_WRITE]  = 1;
    state->permissions[ROLE_ADMIN][ACTION_READ]    = 1;
    state->permissions[ROLE_ADMIN][ACTION_WRITE]   = 1;
    state->permissions[ROLE_ADMIN][ACTION_MANAGE]  = 1;

    if (pthread_mutex_init(&state->state_lock, nullptr) != 0)
        return -2;

    if (sem_init(&state->turnstile, 0, 1) != 0) {
        pthread_mutex_destroy(&state->state_lock);
        return -3;
    }

    if (sem_init(&state->resource_access, 0, 1) != 0) {
        sem_destroy(&state->turnstile);
        pthread_mutex_destroy(&state->state_lock);
        return -4;
    }

    return 0;
}

int sync_destroy(SyncState *state) {
    if (state == nullptr) return -1;

    if (sem_destroy(&state->resource_access) != 0) return -2;
    if (sem_destroy(&state->turnstile) != 0)        return -3;
    if (pthread_mutex_destroy(&state->state_lock) != 0) return -4;

    state->active_readers  = 0;
    state->active_writers  = 0;
    state->waiting_writers = 0;
    state->next_ticket     = 0;
    state->turn_number     = 0;

    for (int i = 0; i < ROLE_COUNT; i++)
        for (int j = 0; j < ACTION_COUNT; j++)
            state->permissions[i][j] = 0;

    return 0;
}

int sync_check_permission(SyncState *state, sync_role_t role,
                          int resource_id, sync_action_t action) {
    if (state == nullptr) return -1;
    if ((int)role   < 0 || (int)role   >= ROLE_COUNT)   return -2;
    if ((int)action < 0 || (int)action >= ACTION_COUNT)  return -2;

    (void)resource_id;

    return state->permissions[(int)role][(int)action] == 1 ? 0 : -1;
}

int sync_begin_read(SyncState *state, sync_role_t role, int resource_id) {
    if (state == nullptr) return -1;
    if (sync_check_permission(state, role, resource_id, ACTION_READ) < 0) return -1;

    if (sem_wait(&state->turnstile) != 0) return -2;

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        sem_post(&state->turnstile);
        return -3;
    }

    if (state->active_readers == 0) {
        if (sem_wait(&state->resource_access) != 0) {
            pthread_mutex_unlock(&state->state_lock);
            sem_post(&state->turnstile);
            return -4;
        }
    }

    state->active_readers++;

    if (pthread_mutex_unlock(&state->state_lock) != 0) {
        sem_post(&state->turnstile);
        return -5;
    }

    if (sem_post(&state->turnstile) != 0) return -6;

    return 0;
}

int sync_end_read(SyncState *state) {
    if (state == nullptr) return -1;

    if (pthread_mutex_lock(&state->state_lock) != 0) return -2;

    if (state->active_readers <= 0) {
        pthread_mutex_unlock(&state->state_lock);
        return -3;
    }

    state->active_readers--;

    if (state->active_readers == 0) {
        if (sem_post(&state->resource_access) != 0) {
            pthread_mutex_unlock(&state->state_lock);
            return -4;
        }
    }

    if (pthread_mutex_unlock(&state->state_lock) != 0) return -5;

    return 0;
}

int sync_begin_write(SyncState *state, sync_role_t role, int resource_id) {
    if (state == nullptr) return -1;
    if (sync_check_permission(state, role, resource_id, ACTION_WRITE) != 0) return -1;

    if (sem_wait(&state->turnstile) != 0) return -2;

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        sem_post(&state->turnstile);
        return -3;
    }

    state->waiting_writers++;

    if (pthread_mutex_unlock(&state->state_lock) != 0) {
        sem_post(&state->turnstile);
        return -4;
    }

    if (sem_wait(&state->resource_access) != 0) {
        pthread_mutex_lock(&state->state_lock);
        state->waiting_writers--;
        pthread_mutex_unlock(&state->state_lock);
        sem_post(&state->turnstile);
        return -5;
    }

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        sem_post(&state->resource_access);
        sem_post(&state->turnstile);
        return -6;
    }

    state->waiting_writers--;
    state->active_writers = 1;

    if (pthread_mutex_unlock(&state->state_lock) != 0) {
        state->active_writers = 0;
        sem_post(&state->resource_access);
        sem_post(&state->turnstile);
        return -7;
    }

    if (sem_post(&state->turnstile) != 0) return -8;

    return 0;
}

int sync_end_write(SyncState *state) {
    if (state == nullptr) return -1;

    if (pthread_mutex_lock(&state->state_lock) != 0) return -2;

    if (state->active_writers <= 0) {
        pthread_mutex_unlock(&state->state_lock);
        return -3;
    }

    state->active_writers--;

    if (sem_post(&state->resource_access) != 0) {
        pthread_mutex_unlock(&state->state_lock);
        return -4;
    }

    if (pthread_mutex_unlock(&state->state_lock) != 0) return -5;

    return 0;
}
