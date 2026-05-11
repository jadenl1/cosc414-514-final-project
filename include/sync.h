#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

//enums

typedef enum {
    ROLE_USER,
    ROLE_EDITOR,
    ROLE_ADMIN,
    ROLE_COUNT
} sync_role_t;

typedef enum {
    ACTION_READ,
    ACTION_WRITE,
    ACTION_MANAGE,
    ACTION_COUNT
} sync_action_t;

//core datat structure

typedef struct {
    int active_readers;
    int active_writers;
    int waiting_writers;
    int next_ticket;
    int turn_number;
    int permissions[ROLE_COUNT][ACTION_COUNT];
    pthread_mutex_t state_lock;
    sem_t           turnstile;
    sem_t           resource_access;
} SyncState;

//api
//
// Return codes: 0 = success, negative = error
//   -1  invalid argument or permission denied
//   -2  invalid role or action
//   -3 and below: OS primitive failure (see function for specifics)

// Lifecycle
int sync_init(SyncState *state);
int sync_destroy(SyncState *state);

// Access control
int sync_check_permission(SyncState *state, sync_role_t role,
                          int resource_id, sync_action_t action);

// Readers-Writers
int sync_begin_read(SyncState *state, sync_role_t role, int resource_id);
int sync_end_read(SyncState *state);

int sync_begin_write(SyncState *state, sync_role_t role, int resource_id);
int sync_end_write(SyncState *state);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_H */
