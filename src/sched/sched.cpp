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
    if (initialized) return -1; // Invalid argument or operation not permitted
    process_table.clear();
    gantt_log.clear();
    current_algo = ALGO_FCFS;
    rr_quantum   = 1;
    next_pid     = 0;
    initialized  = true;
    return 0;
}

int sched_destroy(void) {
    if (!initialized) return -3; // Subsystem not initialized
    process_table.clear();
    gantt_log.clear();
    initialized = false;
    return 0;
}

int sched_reset(void) {
    if (!initialized) return -3; // Subsystem not initialized
    process_table.clear();
    gantt_log.clear();
    next_pid = 0;
    // algorithm config is intentionally preserved across reset
    return 0;
}

// ── Process management ────────────────────────────────────────────────────────

// given the params, create a PCB block with new state info and add it to the process table
int sched_create_process(int arrival_time, int burst_time, int priority) {
    // handle errors
    if (!initialized)    return -3; // Subsystem not initialized
    // Invalid argument or operation not permitted
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

    process_table.push_back(p); // adding new PCB to global process table
    return p.pid;
}

// update a specific process IN process table state -> Terminated
int sched_terminate_process(int pid) {
    if (!initialized) return -3;

    for (PCB &p : process_table) {
        if (p.pid == pid) {
            if (p.state == STATE_TERMINATED) return -1; // Invalid argument or operation not permitted
            p.state = STATE_TERMINATED;
            return 0;
        }
    }
    return -2; // Resource not found
}

// ── Algorithm helpers (internal) ──────────────────────────────────────────────

// copy back results from local procs table -> global process table
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

// ── MAIN 3 ALGORITHMS ──────────────────────────────────────────────

// FIRST COME FIRST SERVE
static void run_fcfs(void) {
    std::vector<PCB> procs;
    // copy all non-terminated procs from process_table -> local procs
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p);

    // sort by procs by function by_arrival_then_pid()
    std::sort(procs.begin(), procs.end(), by_arrival_then_pid);
    // sort by arrival for first come first serve, sort by pid to break ties

    int clock = 0; // initialize clock
    // go through all procs
    for (PCB &p : procs) {
        // handle case: no process arrived at the moment
        if (clock < p.arrival_time) {
            gantt_log.push_back({-1, clock, p.arrival_time}); // log the CPU idle gap from clock -> next arrival time
            clock = p.arrival_time; // jump clock forward to arrival time
        }

        // run the PCB
        p.start_time      = clock;
        p.state           = STATE_RUNNING; // update process state -> Running
        gantt_log.push_back({p.pid, clock, clock + p.burst_time}); // log to gantt
        clock            += p.burst_time; // move clock forward
        p.finish_time     = clock; // record everything
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        // done!
        p.state           = STATE_TERMINATED; // update process state -> Terminated
    }
    writeback(procs); // push changes back to global process table
}

// ROUND ROBIN
static void run_rr(void) {
    // copy all non-terminated process from global process table -> local procs
    std::vector<PCB> procs;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p);

    // sort by procs by function by_arrival_then_pid()
    std::sort(procs.begin(), procs.end(), by_arrival_then_pid);
    // sort by arrival for first come first serve, sort by pid to break ties


    int n = (int)procs.size();
    std::vector<bool> arrived(n, false); // boolean array, tracking which proc has been added to ready so they dont get added twice.
    std::deque<int>   ready; // indicies of PCBs stored in a ready queue
    int clock = 0;

    // initialize ready queue w all proccesses that arrive at 0 & are Ready
    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time <= clock) {
            procs[i].state = STATE_READY;
            arrived[i]     = true;
            ready.push_back(i);
        }
    }

    while (true) {
        // each while loop, first check if we have completed all processes -> break if true
        bool all_done = true;
        for (int i = 0; i < n; i++)
            if (procs[i].state != STATE_TERMINATED) { all_done = false; break; }
        if (all_done) break;

        // handle case where ready is empty (no process arrived at the moment)
        if (ready.empty()) {
            // find next coming arrival time
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!arrived[i]) next = std::min(next, procs[i].arrival_time);
            if (next == INT_MAX) break;
            gantt_log.push_back({-1, clock, next}); // log the CPU idle gap from clock -> next arrival time
            clock = next;
            
            // now ready all processes that are before/at the current clock time
            for (int i = 0; i < n; i++) {
                if (!arrived[i] && procs[i].arrival_time <= clock) { // dont add any that have already been added
                    procs[i].state = STATE_READY;
                    arrived[i] = true;
                    ready.push_back(i);
                }
            }
        }

        // if ready STILL has no processes, we break
        if (ready.empty()) break;

        // pop PCB from ready queue
        int  idx = ready.front(); ready.pop_front();
        PCB &p   = procs[idx];

        // move start time to current clock time, get ready to run PCB
        if (p.start_time == -1) p.start_time = clock;
        p.state = STATE_RUNNING; // run PCB, update state -> Running

        // run until either process is complete or until quantum (whichever is lower)
        int slice = std::min(p.remaining_time, rr_quantum);
        gantt_log.push_back({p.pid, clock, clock + slice}); // log execution
        clock += slice; // push clock forward
        p.remaining_time -= slice;

        // queue newly arrived before re-queueing current
        for (int i = 0; i < n; i++) {
            if (!arrived[i] && procs[i].arrival_time <= clock) {
                procs[i].state = STATE_READY;
                arrived[i]     = true;
                ready.push_back(i);
            }
        }

        // check if this execution completed the Process or if it needs to execute again
        if (p.remaining_time > 0) {
            p.state = STATE_READY;
            ready.push_back(idx); // if it needs more exection, ready it
        } else {
            // else record times and terminate
            p.finish_time     = clock;
            p.turnaround_time = p.finish_time - p.arrival_time;
            p.waiting_time    = p.turnaround_time - p.burst_time;
            p.state           = STATE_TERMINATED; // update state -> Terminate
        }
    }
    writeback(procs); // push local changes back to the global process table
}

static void run_priority(void) {
    std::vector<PCB> procs;
    // Find all processes that are not terminated
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) procs.push_back(p); // add those to procs

    // initialize size of procs
    int n     = (int)procs.size();
    int clock = 0;
    int done  = 0;

    // go through all procs
    while (done < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (procs[i].state == STATE_TERMINATED) continue; // proc has exited
            if (procs[i].arrival_time > clock)      continue; // proc hasn't shown up yet
            if (best == -1) { best = i; continue; } // no best proc has been chosen yet, choose this one
            
            // find proc with highest priority, compare each proc c with current best proc b
            const PCB &b = procs[best], &c = procs[i];
            if (c.priority < b.priority ||
               (c.priority == b.priority && c.arrival_time < b.arrival_time) ||
               (c.priority == b.priority && c.arrival_time == b.arrival_time
                                         && c.pid < b.pid))
                best = i; // if we found a winner, set it to best
        }

        // no processes have arrived OR are terminated
        // this is pure CPU idle case
        if (best == -1) {
            int next = INT_MAX;
            // go through all procs
            for (int i = 0; i < n; i++)
                // find the next non-terminated proc
                if (procs[i].state != STATE_TERMINATED)
                    next = std::min(next, procs[i].arrival_time); // find minimum arrival time
            if (next == INT_MAX) break; // if all procs are terminated, break
            gantt_log.push_back({-1, clock, next}); // log the CPU idle gap from clock -> next arrival time
            clock = next; // jump our clock to the next minimum arrival time
            continue;
        }

        // run the process block that we found
        PCB &p = procs[best];
        if (p.start_time == -1) p.start_time = clock; 
        p.state = STATE_RUNNING; // update state -> running
        gantt_log.push_back({p.pid, clock, clock + p.burst_time}); // log it in gantt
        clock            += p.burst_time; // push clock foward
        p.finish_time     = clock; // keep track of when execution finished
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        // done!
        p.state           = STATE_TERMINATED; // updated state -> terminated
        done++;
    }
    writeback(procs); // push changes back to global process table
}

// ── Algorithm configuration ───────────────────────────────────────────────────

// handle schedule algorithm change
int sched_set_algorithm(SchedAlgo algo, int quantum) {
    if (!initialized)                  return -3;
    if (algo == ALGO_RR && quantum <= 0) return -1; // bad input
    current_algo = algo;
    if (algo == ALGO_RR) rr_quantum = quantum;
    return 0;
}

// ── Simulation ────────────────────────────────────────────────────────────────

int sched_run(void) {
    if (!initialized) return -3;

    // run through process table and make sure there are processes that can run/be scheduled
    int schedulable = 0;
    for (const PCB &p : process_table)
        if (p.state != STATE_TERMINATED) schedulable++;
    if (schedulable == 0) return -1; // if not, then bad input

    // we can run if we reach here

    gantt_log.clear();

    // run the respective algorithm set by user stored in current_algo global var
    switch (current_algo) {
        case ALGO_FCFS:     run_fcfs();     break;
        case ALGO_RR:       run_rr();       break;
        case ALGO_PRIORITY: run_priority(); break;
        default: return -1; // bad input
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
