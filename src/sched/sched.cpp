#include "sched.h"
#include <vector>
#include <deque>
#include <algorithm>
#include <climits>
#include <cstring>

// ── Internal state ────────────────────────────────────────────────────────────

static bool         initialized   = false;
static int          next_pid      = 0;
static SchedAlgo    current_algo  = ALGO_FCFS;
static int          rr_quantum    = 1;

static std::vector<PCB>        process_table;
static std::vector<GanttEntry> gantt_log;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

int sched_init(void) {
    if (initialized) return -1;
    process_table.clear();
    gantt_log.clear();
    current_algo = ALGO_FCFS;
    rr_quantum   = 1;
    next_pid     = 0;
    initialized  = true;
    return 0;
}

int sched_destroy(void) {
    if (!initialized) return -3;
    process_table.clear();
    gantt_log.clear();
    initialized = false;
    return 0;
}

int sched_reset(void) {
    if (!initialized) return -3;
    process_table.clear();
    gantt_log.clear();
    next_pid = 0;
    // algorithm config is intentionally preserved across reset
    return 0;
}

// ── Process management ────────────────────────────────────────────────────────

int sched_create_process(int arrival_time, int burst_time, int priority) {
    if (!initialized)    return -3;
    if (burst_time <= 0) return -1;
    if (arrival_time < 0) return -1;

    PCB p;
    p.pid            = next_pid++;
    p.arrival_time   = arrival_time;
    p.burst_time     = burst_time;
    p.remaining_time = burst_time;
    p.priority       = priority;
    p.state          = STATE_NEW;
    p.start_time     = -1;
    p.finish_time    = -1;
    p.waiting_time   = 0;
    p.turnaround_time = 0;

    process_table.push_back(p);
    return p.pid;
}

int sched_terminate_process(int pid) {
    if (!initialized) return -3;

    for (PCB &p : process_table) {
        if (p.pid == pid) {
            if (p.state == STATE_TERMINATED) return -1;
            p.state = STATE_TERMINATED;
            return 0;
        }
    }
    return -2;
}

// ── Algorithm helpers (internal) ──────────────────────────────────────────────

static void writeback(const std::vector<PCB> &procs) {
    for (const PCB &p : procs)
        for (PCB &orig : process_table)
            if (orig.pid == p.pid) { orig = p; break; }
}

static bool by_arrival_then_pid(const PCB &a, const PCB &b) {
    return a.arrival_time != b.arrival_time
               ? a.arrival_time < b.arrival_time
               : a.pid < b.pid;
}

static void run_fcfs(void) {
    std::vector<PCB> procs;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p);

    std::sort(procs.begin(), procs.end(), by_arrival_then_pid);

    int clock = 0;
    for (PCB &p : procs) {
        if (clock < p.arrival_time) {
            gantt_log.push_back({-1, clock, p.arrival_time});
            clock = p.arrival_time;
        }
        p.start_time      = clock;
        p.state           = STATE_RUNNING;
        gantt_log.push_back({p.pid, clock, clock + p.burst_time});
        clock            += p.burst_time;
        p.finish_time     = clock;
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        p.state           = STATE_TERMINATED;
    }
    writeback(procs);
}

static void run_rr(void) {
    std::vector<PCB> procs;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p);

    std::sort(procs.begin(), procs.end(), by_arrival_then_pid);

    int n = (int)procs.size();
    std::vector<bool> arrived(n, false);
    std::deque<int>   ready;
    int clock = 0;

    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time <= clock) {
            procs[i].state = STATE_READY;
            arrived[i]     = true;
            ready.push_back(i);
        }
    }

    while (true) {
        bool all_done = true;
        for (int i = 0; i < n; i++)
            if (procs[i].state != STATE_TERMINATED) { all_done = false; break; }
        if (all_done) break;

        if (ready.empty()) {
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!arrived[i]) next = std::min(next, procs[i].arrival_time);
            if (next == INT_MAX) break;
            gantt_log.push_back({-1, clock, next});
            clock = next;
            for (int i = 0; i < n; i++) {
                if (!arrived[i] && procs[i].arrival_time <= clock) {
                    procs[i].state = STATE_READY;
                    arrived[i]     = true;
                    ready.push_back(i);
                }
            }
        }

        if (ready.empty()) break;

        int  idx = ready.front(); ready.pop_front();
        PCB &p   = procs[idx];

        if (p.start_time == -1) p.start_time = clock;
        p.state = STATE_RUNNING;

        int slice = std::min(p.remaining_time, rr_quantum);
        gantt_log.push_back({p.pid, clock, clock + slice});
        clock            += slice;
        p.remaining_time -= slice;

        // enqueue newly arrived before re-enqueueing current
        for (int i = 0; i < n; i++) {
            if (!arrived[i] && procs[i].arrival_time <= clock) {
                procs[i].state = STATE_READY;
                arrived[i]     = true;
                ready.push_back(i);
            }
        }

        if (p.remaining_time > 0) {
            p.state = STATE_READY;
            ready.push_back(idx);
        } else {
            p.finish_time     = clock;
            p.turnaround_time = p.finish_time - p.arrival_time;
            p.waiting_time    = p.turnaround_time - p.burst_time;
            p.state           = STATE_TERMINATED;
        }
    }
    writeback(procs);
}

static void run_priority(void) {
    std::vector<PCB> procs;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p);

    int n     = (int)procs.size();
    int clock = 0;
    int done  = 0;

    while (done < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].state == STATE_TERMINATED) continue;
            if (procs[i].arrival_time > clock)      continue;
            if (best == -1) { best = i; continue; }
            const PCB &b = procs[best], &c = procs[i];
            if (c.priority < b.priority ||
               (c.priority == b.priority && c.arrival_time < b.arrival_time) ||
               (c.priority == b.priority && c.arrival_time == b.arrival_time
                                         && c.pid < b.pid))
                best = i;
        }

        if (best == -1) {
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (procs[i].state != STATE_TERMINATED)
                    next = std::min(next, procs[i].arrival_time);
            if (next == INT_MAX) break;
            gantt_log.push_back({-1, clock, next});
            clock = next;
            continue;
        }

        PCB &p = procs[best];
        if (p.start_time == -1) p.start_time = clock;
        p.state = STATE_RUNNING;
        gantt_log.push_back({p.pid, clock, clock + p.burst_time});
        clock            += p.burst_time;
        p.finish_time     = clock;
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        p.state           = STATE_TERMINATED;
        done++;
    }
    writeback(procs);
}

// ── Algorithm configuration ───────────────────────────────────────────────────

int sched_set_algorithm(SchedAlgo algo, int quantum) {
    if (!initialized)                  return -3;
    if (algo == ALGO_RR && quantum <= 0) return -1;
    current_algo = algo;
    if (algo == ALGO_RR) rr_quantum = quantum;
    return 0;
}

// ── Simulation ────────────────────────────────────────────────────────────────

int sched_run(void) {
    if (!initialized) return -3;

    int schedulable = 0;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) schedulable++;
    if (schedulable == 0) return -1;

    gantt_log.clear();

    switch (current_algo) {
        case ALGO_FCFS:     run_fcfs();     break;
        case ALGO_RR:       run_rr();       break;
        case ALGO_PRIORITY: run_priority(); break;
        default: return -1;
    }
    return 0;
}

// ── Stats & Gantt accessors ───────────────────────────────────────────────────

int sched_get_stats(int pid, int *waiting_time, int *turnaround_time) {
    if (!initialized)          return -3;
    if (!waiting_time || !turnaround_time) return -1;

    for (const PCB &p : process_table) {
        if (p.pid == pid) {
            *waiting_time    = p.waiting_time;
            *turnaround_time = p.turnaround_time;
            return 0;
        }
    }
    return -2;
}

int sched_get_avg_waiting_time(double *out) {
    if (!initialized) return -3;
    if (!out)         return -1;
    if (process_table.empty()) return -1;

    double sum = 0;
    for (const PCB &p : process_table) sum += p.waiting_time;
    *out = sum / (double)process_table.size();
    return 0;
}

int sched_get_avg_turnaround_time(double *out) {
    if (!initialized) return -3;
    if (!out)         return -1;
    if (process_table.empty()) return -1;

    double sum = 0;
    for (const PCB &p : process_table) sum += p.turnaround_time;
    *out = sum / (double)process_table.size();
    return 0;
}

int sched_get_gantt(GanttEntry *entries, int *count) {
    if (!initialized) return -3;
    if (!count)       return -1;

    int n = (int)gantt_log.size();
    if (!entries) { *count = n; return 0; }  // size-query mode
    if (*count < n) return -1;               // buffer too small

    for (int i = 0; i < n; i++) entries[i] = gantt_log[i];
    *count = n;
    return 0;
}
