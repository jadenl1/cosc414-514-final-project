#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

typedef enum {
    ROLE_USER;
    ROLE_EDITOR;
    ROLE_ADMIN;
    ROLE_COUNT;
} sync_role_t;

typedef enum {
    ACTION_READ;
    ACTION_WRITE;
    ACTION_MANAGE;
    ACTION_COUNT;
} sync_action_t;

typedef struct {
    int active_readers;
    int active_writers;
    int waiting_writers;
    int next_ticket;
    int turn_number;

    int permissions[ROLE_COUNT][ACTION_COUNT];

    pthread_mutex_t state_lock;
    sem_t turnstile;
    sem_t resource_access;
} SyncState;

int sync_init(SyncState *state) {
    int i, j;

    if (state == nullptr) {
        return -1;
    }

    state->active_readers = 0;
    state->active_writers = 0;
    state->waiting_writers = 0;
    state->next_ticket = 0;
    state->turn_number = 0;

    for (i = 0; i < ROLE_COUNT; i++) {
        for (j = 0; j < ACTION_COUNT; j++) {
            state->permissions[i][j] = 0;
        }
    }

    state->permissions[ROLE_USER][ACTION_READ] = 1;

    state->permissions[ROLE_EDITOR][ACTION_READ] = 1;
    state->permissions[ROLE_EDITOR][ACTION_WRITE] = 1;

    state->permissions[ROLE_ADMIN][ACTION_READ] = 1;
    state->permissions[ROLE_ADMIN][ACTION_WRITE] = 1;
    state->permissions[ROLE_ADMIN][ACTION_MANAGE] = 1;


    if (pthread_mutex_init(&state->state_lock, nullptr) != 0) {
        return -2;
    }

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

int sync_check_permission(SyncState *state, sync_role_t role, int resource_id, sync_action_t action) {
    if (state == nullptr) {
        return -2;
    }

    if (static_cast<int>(role) < 0 || static_cast<int>(role) >= ROLE_COUNT) {
        return -2;
    }

    if (static_cast<int>(action) < 0 || static_cast<int>(action) >= ACTION_COUNT) {
        return -2;
    }

    (void)resource_id;

    if (state->permissions[static_cast<int>(role)][static_cast<int>(action)] == 1) {
        return 0;
    }

    return -1;
}

int sync_begin_read(SyncState *state, sync_role_t role, int resource_id) {
    if (state == nullptr) {
        return -1;
    }

    if (sync_check_permission(state, role, resource_id, ACTION_READ) < 0) {
        return -1;
    }

    if (sem_wait(&state->turnstile) != 0) {
        return -2;
    }

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        sem_post(&state->turnstile);
        return -3;
    }

    /* First reader acquires resource_access so writers are blocked while readers are active */
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

    if (sem_post(&state->turnstile) != 0) {
        return -6;
    }

    return 0;
}

int sync_end_read(SyncState *state) {
    if (state == nullptr) {
        return -1;
    }

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        return -2;
    }

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

    if (pthread_mutex_unlock(&state->state_lock) != 0) {
    	return -5;
    }

    return 0;
}

int sync_begin_write(SyncState *state, sync_role_t role, int resource_id) {
    if (state == nullptr) {
        return -1;
    }

    if (sync_check_permission(state, role, resource_id, ACTION_WRITE) != 0) {
        return -1;
    }

    if (sem_wait(&state->turnstile) != 0) {
        return -2;
    }

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
    	state->waiting_writers--; // Remove the failed writer from the wait count
    	pthread_mutex_unlock(&state->state_lock);
    	sem_post(&state->turnstile); // Release the turnstile so others aren't blocked
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

    if (sem_post(&state->turnstile) != 0) {
        return -8;
    }

    return 0;
}

int sync_end_write(SyncState *state) {
    if (state == nullptr) {
        return -1;
    }

    if (pthread_mutex_lock(&state->state_lock) != 0) {
        return -2;
    }

    if (state->active_writers <= 0) {
        pthread_mutex_unlock(&state->state_lock);
        return -3;
    }

    state->active_writers--;

    if (sem_post(&state->resource_access) != 0) {
        pthread_mutex_unlock(&state->state_lock);
        return -4;
    }

    if (pthread_mutex_unlock(&state->state_lock) != 0) {
    	return -5;
    }

    return 0;
}

int sync_destroy(SyncState *state) {
    if (state == nullptr) {
        return -1;
    }

    if (sem_destroy(&state->resource_access) != 0) {
        return -2;
    }

    if (sem_destroy(&state->turnstile) != 0) {
        return -3;
    }

    if (pthread_mutex_destroy(&state->state_lock) != 0) {
        return -4;
    }

    state->active_readers = 0;
    state->active_writers = 0;
    state->waiting_writers = 0;
    state->next_ticket = 0;
    state->turn_number = 0;

    int role, action;

    for (role = 0; role < ROLE_COUNT; role++) {
        for (action = 0; action < ACTION_COUNT; action++) {
            state->permissions[role][action] = 0;
        }
    }

    return 0;
}