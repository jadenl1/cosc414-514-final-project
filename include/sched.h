#ifndef SCHED_H
#define SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Enums ────────────────────────────────────────────────────────────────────

typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_TERMINATED
} ProcessState;

typedef enum {
    ALGO_FCFS,
    ALGO_RR,
    ALGO_PRIORITY
} SchedAlgo;

// ── Core data structures ──────────────────────────────────────────────────────

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;        // lower value = higher priority
    ProcessState state;
    int start_time;      // set on first dispatch
    int finish_time;     // set on termination
    int waiting_time;    // computed after sched_run()
    int turnaround_time; // computed after sched_run()
} PCB;

typedef struct {
    int pid;        // -1 indicates CPU idle
    int start_time;
    int end_time;
} GanttEntry;

// ── API ───────────────────────────────────────────────────────────────────────
//
// Return codes: 0 = success, negative = error
//   -1  generic error / invalid argument
//   -2  not found
//   -3  subsystem not initialized

// Lifecycle
int sched_init(void);
int sched_destroy(void);
int sched_reset(void);

// Process management
// Returns new pid (>= 0) on success, -1 on invalid args (burst_time <= 0)
int sched_create_process(int arrival_time, int burst_time, int priority);
int sched_terminate_process(int pid);

// Algorithm configuration
// quantum is ignored for SCHED_FCFS and SCHED_PRIORITY
int sched_set_algorithm(SchedAlgo algo, int quantum);

// Simulation — must call sched_set_algorithm() first
int sched_run(void);

// Per-process stats — valid after sched_run()
int sched_get_stats(int pid, int *waiting_time, int *turnaround_time);

// Aggregate stats — valid after sched_run()
int sched_get_avg_waiting_time(double *out);
int sched_get_avg_turnaround_time(double *out);

// Gantt timeline — fills caller-provided array, sets *count to number of entries
// Call with entries=NULL and *count=0 to query required array size first
int sched_get_gantt(GanttEntry *entries, int *count);

#ifdef __cplusplus
}
#endif

#endif /* SCHED_H */
