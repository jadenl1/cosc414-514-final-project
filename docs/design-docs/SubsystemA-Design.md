# Subsystem A Design Document
## Process Management & CPU Scheduling

**Author:** Jaden Leonard
**Course:** COSC 514 – Operating Systems
**Project:** MOSS – Mini Operating System Services Simulator

---

## 1. Overview

Subsystem A simulates CPU scheduling in user space. It maintains a table of
processes (each represented by a Process Control Block), accepts a scheduling
algorithm selection, runs the simulation forward in discrete time steps, and
exposes per-process and aggregate statistics to the rest of the system through
a stable C API.

All scheduling is **logical** — no real threads or timers are used. Time is
represented as an integer clock that advances by burst or quantum units.

---

## 2. Data Structures

### 2.1 `ProcessState` (enum)

```c
typedef enum { STATE_NEW, STATE_READY, STATE_RUNNING, STATE_TERMINATED } ProcessState;
```

Tracks the lifecycle of a process. A process starts as `STATE_NEW` when
created, transitions to `STATE_READY` when it enters the ready queue,
`STATE_RUNNING` when dispatched by the scheduler, and `STATE_TERMINATED` when
it completes or is explicitly terminated via `sched_terminate_process()`.

### 2.2 `SchedAlgo` (enum)

```c
typedef enum { ALGO_FCFS, ALGO_RR, ALGO_PRIORITY } SchedAlgo;
```

Selects the active scheduling algorithm. Named with the `ALGO_` prefix to
avoid collision with the POSIX `SCHED_RR` macro defined in `<pthread.h>`.

### 2.3 `PCB` (struct)

| Field | Type | Description |
|---|---|---|
| `pid` | `int` | Unique process ID, assigned sequentially from 0 |
| `arrival_time` | `int` | Time unit at which the process enters the system |
| `burst_time` | `int` | Total CPU time required (immutable after creation) |
| `remaining_time` | `int` | CPU time still needed; decremented during RR simulation |
| `priority` | `int` | Scheduling priority — **lower value = higher priority** |
| `state` | `ProcessState` | Current lifecycle state |
| `start_time` | `int` | Clock value when first dispatched; -1 until first run |
| `finish_time` | `int` | Clock value when process completes |
| `waiting_time` | `int` | Time spent waiting in the ready queue |
| `turnaround_time` | `int` | Total time from arrival to completion |

`waiting_time` and `turnaround_time` are computed after `sched_run()` using:

```
turnaround_time = finish_time - arrival_time
waiting_time    = turnaround_time - burst_time
```

### 2.4 `GanttEntry` (struct)

```c
typedef struct { int pid; int start_time; int end_time; } GanttEntry;
```

Represents one contiguous slot in the scheduling timeline. A `pid` of `-1`
indicates the CPU was idle during that interval. The Gantt log is built
internally during `sched_run()` and retrieved by the caller via
`sched_get_gantt()` for display — no printing occurs inside the subsystem.

---

## 3. Algorithm Design

### 3.1 FCFS (First-Come, First-Served)

Processes are sorted by `arrival_time`, with `pid` as the tie-breaker for
simultaneous arrivals (lower pid runs first, giving deterministic output).
Each process runs to completion before the next is dispatched.

If the CPU would sit idle between the end of one process and the arrival of the
next, an idle `GanttEntry` (pid = -1) is inserted and the clock jumps forward
to the next arrival time.

FCFS is non-preemptive by definition. It is the simplest algorithm and serves
as the baseline for comparing wait times against RR and Priority.

### 3.2 Round Robin

Processes are placed in a FIFO ready queue (`std::deque`), sorted initially by
arrival time. The scheduler dispatches the front process for `min(remaining_time,
quantum)` time units, then checks for newly arrived processes and enqueues them
before re-enqueueing the current process (if it still has remaining time). This
ordering ensures a process that arrives exactly when a quantum expires enters
the queue ahead of the preempted process, matching standard RR behavior.

`start_time` is recorded only on the **first** dispatch so that response time
can be derived if needed. If the ready queue empties before all processes are
done (i.e., all remaining processes arrive in the future), an idle entry is
inserted and the clock jumps to the next arrival.

The time quantum is configurable via `sched_set_algorithm(ALGO_RR, quantum)`
and must be greater than 0.

### 3.3 Priority Scheduling (Non-Preemptive)

At each scheduling point the algorithm scans all non-terminated processes that
have arrived at or before the current clock and selects the one with the
**lowest priority number** (highest urgency). Ties are broken first by
`arrival_time`, then by `pid`.

The choice of **non-preemptive** priority was deliberate: it keeps the
implementation correct and predictable without the added complexity of
mid-burst preemption, which would require saving and restoring CPU context —
unnecessary in a logical simulator. The trade-off is that a high-priority
process that arrives while a lower-priority process is running must wait until
the current process finishes.

As with FCFS and RR, idle entries are inserted when no process is available at
the current clock value.

---

## 4. API Reference

All functions return `0` on success and a negative value on failure:
- `-1` — invalid argument or operation not permitted
- `-2` — process not found
- `-3` — subsystem not initialized

No `sched_` function prints to stdout or stderr. All output is the
responsibility of the caller.

---

### Lifecycle

#### `int sched_init(void)`
Initializes internal state. Must be called before any other function.
Returns `-1` if already initialized.

#### `int sched_destroy(void)`
Frees all internal state. Returns `-3` if not initialized.

#### `int sched_reset(void)`
Clears all processes and the Gantt log. The algorithm configuration is
preserved. Returns `-3` if not initialized.

---

### Process Management

#### `int sched_create_process(int arrival_time, int burst_time, int priority)`
Creates a new process and adds it to the process table.
- `arrival_time` must be ≥ 0; `burst_time` must be > 0
- Returns the new `pid` (≥ 0) on success, `-1` on invalid arguments, `-3` if not initialized

#### `int sched_terminate_process(int pid)`
Marks a process as `STATE_TERMINATED` before `sched_run()` is called,
effectively removing it from the simulation.
- Returns `-1` if already terminated, `-2` if pid not found, `-3` if not initialized

---

### Configuration & Simulation

#### `int sched_set_algorithm(SchedAlgo algo, int quantum)`
Sets the active scheduling algorithm. `quantum` is only used for `ALGO_RR`
and must be > 0; it is ignored for `ALGO_FCFS` and `ALGO_PRIORITY`.
Returns `-1` if `ALGO_RR` is selected with `quantum ≤ 0`.

#### `int sched_run(void)`
Runs the scheduling simulation on the current process table. Clears and
rebuilds the Gantt log and computes all statistics. Returns `-1` if there
are no schedulable processes, `-3` if not initialized.

---

### Statistics & Output

#### `int sched_get_stats(int pid, int *waiting_time, int *turnaround_time)`
Writes per-process stats into the provided pointers after `sched_run()`.
Returns `-1` if either pointer is NULL, `-2` if pid not found.

#### `int sched_get_avg_waiting_time(double *out)`
#### `int sched_get_avg_turnaround_time(double *out)`
Write aggregate averages across all processes. Return `-1` if `out` is NULL
or the process table is empty.

#### `int sched_get_gantt(GanttEntry *entries, int *count)`
Retrieves the Gantt timeline built by `sched_run()`.
- **Size-query mode:** call with `entries = NULL`; `*count` is set to the number of entries
- **Fill mode:** provide a buffer; `*count` is set to entries written
- Returns `-1` if `count` is NULL or the buffer is too small

---

## 5. Quantitative Performance Evaluation

To satisfy the COSC 514 graduate requirement for quantitative analysis, the three
scheduling algorithms were evaluated on the same fixed workload and their performance
metrics compared.

### 5.1 Test Workload

Five processes with varied arrival times, burst times, and priorities:

| PID | Arrival | Burst | Priority |
|-----|---------|-------|----------|
| P0  | 0       | 8     | 3        |
| P1  | 1       | 4     | 1 (highest) |
| P2  | 2       | 9     | 4        |
| P3  | 3       | 5     | 2        |
| P4  | 4       | 2     | 5 (lowest) |

Total CPU work: 28 time units. No idle gaps (P0 arrives at t=0).

### 5.2 Results

#### FCFS
```
| P0   | P1   | P2   | P3   | P4   |
0     8     12    21    26    28
```

| PID | Wait | Turnaround |
|-----|------|------------|
| P0  | 0    | 8          |
| P1  | 7    | 11         |
| P2  | 10   | 19         |
| P3  | 18   | 23         |
| P4  | 22   | 24         |
| **Avg** | **11.40** | **17.00** |

#### Round Robin (quantum = 3)
```
| P0 | P1 | P2 | P3 | P0 | P4 | P1 | P2 | P3 | P0 | P2 |
0   3   6   9   12  15  17  18  21  23  25  28
```

| PID | Wait | Turnaround |
|-----|------|------------|
| P0  | 17   | 25         |
| P1  | 13   | 17         |
| P2  | 17   | 26         |
| P3  | 15   | 20         |
| P4  | 11   | 13         |
| **Avg** | **14.60** | **20.20** |

#### Priority (non-preemptive, lower number = higher priority)
```
| P0   | P1   | P3   | P2   | P4   |
0     8     12    17    26    28
```

| PID | Wait | Turnaround |
|-----|------|------------|
| P0  | 0    | 8          |
| P1  | 7    | 11         |
| P2  | 15   | 24         |
| P3  | 9    | 14         |
| P4  | 22   | 24         |
| **Avg** | **10.60** | **16.20** |

### 5.3 Comparison Summary

| Algorithm | Avg Wait | Avg Turnaround | Fairness |
|-----------|----------|----------------|----------|
| FCFS      | 11.40    | 17.00          | Poor — later arrivals wait longest regardless of burst |
| RR (q=3)  | 14.60    | 20.20          | Best — every process gets CPU time regularly |
| Priority  | 10.60    | 16.20          | Biased — low-priority processes (P4) still starve |

### 5.4 Analysis

**FCFS** produces the simplest schedule but suffers from the convoy effect: P2 (burst=9)
blocks P3 and P4 even though they are shorter and arrive only slightly later. Its average
wait of 11.40 is moderate — better than RR in this workload because there is no
preemption overhead.

**Round Robin** achieves the best fairness: no process waits more than 17 time units
regardless of burst length. However, its average wait (14.60) and turnaround (20.20) are
the worst of the three. The overhead comes from context switching — P0 is dispatched
three separate times instead of once, increasing its turnaround from 8 (FCFS) to 25.
The quantum size significantly impacts RR performance; a larger quantum reduces
switching overhead at the cost of fairness, converging toward FCFS behavior.

**Priority** yields the best average wait (10.60) and turnaround (16.20) for this
workload because the priority ordering (P1→P3→P2→P4) happens to align well with
arrival order and burst length. However, it carries a starvation risk: if high-priority
processes continuously arrive, P4 (priority=5) could wait indefinitely. In this bounded
workload, P4 waits 22 units — the same as in FCFS. Non-preemptive priority also means
P1 (highest priority, arrives at t=1) cannot interrupt P0, which is already running.

**Conclusion:** No single algorithm is universally optimal. FCFS is predictable and
simple. RR maximizes fairness and response time at the cost of throughput. Priority
minimizes average metrics when priorities are well-assigned but introduces starvation
risk. The choice of algorithm should be driven by the workload's latency vs. fairness
requirements.

---

## 6. Assumptions & Limitations

- All time values are non-negative integers; fractional time is not modeled
- `burst_time` is fixed at creation and does not change during simulation
- Priority scheduling is non-preemptive; a running process cannot be interrupted
- No real concurrency or threads are used; the simulation is single-threaded and deterministic
- `sched_run()` can be called multiple times (e.g., after changing algorithm); each call rebuilds stats and Gantt from the current process table
- Processes pre-terminated via `sched_terminate_process()` are excluded from scheduling but remain in the process table
- The Gantt buffer must be allocated by the caller; use the size-query mode first to determine the required size
