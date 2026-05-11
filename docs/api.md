# MOSS Subsystem Interface Specification

**Course:** COSC 414/514 – Operating Systems
**Team:** Jaden Leonard, Amir Taylor, Fletcher Baccus, Uzoma Bruce
**Project:** Mini Operating System Services Simulator (MOSS)

---

## 1. Shared Conventions

### Return Codes
All API functions return an integer status code:

| Code | Meaning |
|---|---|
| `0` | Success |
| `-1` | Invalid argument or operation not permitted |
| `-2` | Resource not found |
| `-3` | Subsystem not initialized (or OS primitive failure in Subsystem C) |

### Naming Prefixes
| Prefix | Subsystem |
|---|---|
| `sched_` | Process Management & CPU Scheduling |
| `mem_` | Memory Management & Virtual Memory |
| `sync_` | Synchronization & Protection |

### General Rules
- No API function prints to stdout or stderr. All output is the responsibility of the caller.
- Each subsystem must be initialized before any other function in that subsystem is called.
- Subsystems own their internal state. No subsystem directly reads or modifies another's internal data structures — all interaction must go through API calls.
- Error codes must be propagated upward and never silently ignored.

---

## 2. Subsystem A — Process Management & CPU Scheduling

**Header:** `include/sched.h`
**Implementation:** `src/sched/sched.cpp`

### Types

```c
typedef enum { STATE_NEW, STATE_READY, STATE_RUNNING, STATE_TERMINATED } ProcessState;

typedef enum { ALGO_FCFS, ALGO_RR, ALGO_PRIORITY } SchedAlgo;

typedef struct {
    int pid;              // unique process ID (assigned at creation)
    int arrival_time;     // time unit the process enters the system
    int burst_time;       // total CPU time required (immutable)
    int remaining_time;   // CPU time still needed
    int priority;         // lower value = higher priority
    ProcessState state;
    int start_time;       // clock value on first dispatch (-1 until dispatched)
    int finish_time;      // clock value on completion
    int waiting_time;     // computed after sched_run()
    int turnaround_time;  // computed after sched_run()
} PCB;

typedef struct {
    int pid;        // -1 = CPU idle
    int start_time;
    int end_time;
} GanttEntry;
```

### Function Reference

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `sched_init` | `void` | `0` / `-1` | `-1` if already initialized |
| `sched_destroy` | `void` | `0` / `-3` | frees all internal state |
| `sched_reset` | `void` | `0` / `-3` | clears processes and Gantt log; keeps algorithm config |
| `sched_create_process` | `int arrival, int burst, int priority` | `pid ≥ 0` / `-1` / `-3` | `-1` if `burst ≤ 0` or `arrival < 0` |
| `sched_terminate_process` | `int pid` | `0` / `-1` / `-2` / `-3` | `-1` if already terminated; `-2` if pid not found |
| `sched_set_algorithm` | `SchedAlgo algo, int quantum` | `0` / `-1` / `-3` | quantum required and must be `> 0` for `ALGO_RR` |
| `sched_run` | `void` | `0` / `-1` / `-3` | `-1` if no schedulable processes |
| `sched_get_stats` | `int pid, int *wait, int *turnaround` | `0` / `-1` / `-2` / `-3` | valid after `sched_run()`; `-1` if pointers are NULL |
| `sched_get_avg_waiting_time` | `double *out` | `0` / `-1` / `-3` | `-1` if `out` is NULL or table is empty |
| `sched_get_avg_turnaround_time` | `double *out` | `0` / `-1` / `-3` | `-1` if `out` is NULL or table is empty |
| `sched_get_gantt` | `GanttEntry *entries, int *count` | `0` / `-1` / `-3` | pass `entries=NULL` to query count only; `-1` if buffer too small |

---

## 3. Subsystem B — Memory Management & Virtual Memory

**Header:** `include/mem.h`
**Implementation:** `src/mem/mem.cpp`

### Constants

| Constant | Value | Description |
|---|---|---|
| `MEM_DEFAULT_FRAMES` | `4` | number of physical frames |
| `MEM_PAGE_SIZE` | `256` | bytes per page (8-bit offset) |
| `MEM_ADDR_BITS` | `16` | logical address width |
| `MEM_MAX_ADDR` | `65535` | maximum valid logical address |

### Types

```c
typedef enum { MEM_FIFO, MEM_LRU } MemAlgo;
```

### Function Reference

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `mem_init` | `void` | `0` / `-1` | `-1` if already initialized |
| `mem_destroy` | `void` | `0` / `-3` | frees all internal state |
| `mem_reset` | `void` | `0` / `-3` | clears frames and stats; keeps algorithm config |
| `mem_set_algorithm` | `MemAlgo algo` | `0` / `-3` | call before `mem_access()` |
| `mem_access` | `int logical_addr` | `0` / `1` / `-1` / `-3` | `0`=hit, `1`=page fault, `-1`=invalid address |
| `mem_logical_to_physical` | `int logical_addr, int *physical_addr` | `0` / `-1` / `-2` / `-3` | `-2` if page not currently loaded |
| `mem_get_stats` | `int *hits, int *faults` | `0` / `-1` / `-3` | `-1` if either pointer is NULL |
| `mem_get_frames` | `int *frames_out, int *count` | `0` / `-1` / `-3` | pass `frames_out=NULL` to query count only; `-1` in array = empty frame |

---

## 4. Subsystem C — Synchronization & Protection

**Header:** `include/sync.h`
**Implementation:** `src/sync/sync.cpp`

### Types

```c
typedef enum { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN, ROLE_COUNT } sync_role_t;

typedef enum { ACTION_READ, ACTION_WRITE, ACTION_MANAGE, ACTION_COUNT } sync_action_t;

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
```

### Default Permission Matrix

| Role | READ | WRITE | MANAGE |
|---|---|---|---|
| `ROLE_USER` | ✅ | ❌ | ❌ |
| `ROLE_EDITOR` | ✅ | ✅ | ❌ |
| `ROLE_ADMIN` | ✅ | ✅ | ✅ |

### Function Reference

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `sync_init` | `SyncState *state` | `0` / `-1` to `-4` | `-1` if `state` is NULL; lower codes = OS primitive failure |
| `sync_destroy` | `SyncState *state` | `0` / `-1` to `-4` | must be called to release mutex and semaphores |
| `sync_check_permission` | `SyncState *state, sync_role_t role, int resource_id, sync_action_t action` | `0` / `-1` / `-2` | `0`=allowed, `-1`=denied, `-2`=invalid role or action |
| `sync_begin_read` | `SyncState *state, sync_role_t role, int resource_id` | `0` / `-1` to `-6` | blocks if a writer holds the resource |
| `sync_end_read` | `SyncState *state` | `0` / `-1` to `-5` | `-3` if no active readers |
| `sync_begin_write` | `SyncState *state, sync_role_t role, int resource_id` | `0` / `-1` to `-8` | blocks until no readers or writers hold the resource |
| `sync_end_write` | `SyncState *state` | `0` / `-1` to `-5` | `-3` if no active writers |

> **Platform note:** Subsystem C uses POSIX semaphores (`sem_init`, `sem_destroy`) which require Ubuntu 22.04. These functions are not supported on macOS.

---

## 5. Shared Assumptions & Constraints

- **Time is discrete integer-valued.** All arrival times, burst times, and clock values are non-negative integers. Fractional time is not modeled.
- **No real concurrency.** Subsystems A and B are single-threaded simulations. Subsystem C simulates synchronization using POSIX primitives but does not require real parallel execution.
- **Initialization order.** Each subsystem must be initialized independently before use (`sched_init`, `mem_init`, `sync_init`). There is no global MOSS initializer in Part I.
- **No cross-subsystem global variables.** Subsystems do not share memory or global state. All interaction in Part II must go through API function calls.
- **Platform.** Ubuntu Linux 22.04 LTS, g++ version 11 or later, standard POSIX libraries only.
- **Build.** `make` produces the unified `moss` binary. `make demos` produces individual Part I demo binaries. `make test` runs all tests.
