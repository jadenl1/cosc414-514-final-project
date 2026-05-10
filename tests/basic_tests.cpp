#include <iostream>
#include "sched.h"

static int passed = 0;
static int failed = 0;

#define CHECK(label, expr) \
    do { \
        if (expr) { std::cout << "[PASS] " << label << "\n"; ++passed; } \
        else       { std::cout << "[FAIL] " << label << "\n"; ++failed; } \
    } while(0)

// ── Section 1: Lifecycle ──────────────────────────────────────────────────────

static void test_lifecycle() {
    std::cout << "\n-- Lifecycle --\n";

    CHECK("init succeeds",          sched_init()    == 0);
    CHECK("double init fails",      sched_init()    == -1);
    CHECK("reset when initialized", sched_reset()   == 0);
    CHECK("destroy succeeds",       sched_destroy() == 0);
    CHECK("reset when not inited",  sched_reset()   == -3);
    CHECK("destroy when not inited",sched_destroy() == -3);
    CHECK("reinit after destroy",   sched_init()    == 0);

    sched_destroy();
}

// ── Section 2: Process management ────────────────────────────────────────────

static void test_process_management() {
    std::cout << "\n-- Process Management --\n";

    sched_init();

    // valid creation
    int pid0 = sched_create_process(0, 5, 1);
    int pid1 = sched_create_process(1, 3, 2);
    int pid2 = sched_create_process(2, 8, 0);
    CHECK("first pid is 0",          pid0 == 0);
    CHECK("second pid is 1",         pid1 == 1);
    CHECK("third pid is 2",          pid2 == 2);

    // invalid args
    CHECK("zero burst rejected",     sched_create_process(0,  0, 1) == -1);
    CHECK("negative burst rejected", sched_create_process(0, -1, 1) == -1);
    CHECK("negative arrival rejected",sched_create_process(-1, 5, 1) == -1);

    // termination
    CHECK("terminate existing",      sched_terminate_process(pid0) == 0);
    CHECK("terminate again fails",   sched_terminate_process(pid0) == -1);
    CHECK("terminate unknown pid",   sched_terminate_process(99)   == -2);

    // uninitialized guards
    sched_destroy();
    CHECK("create when not inited",  sched_create_process(0, 5, 1) == -3);
    CHECK("terminate when not inited",sched_terminate_process(0)   == -3);
}

// ── Section 3a: Stat & Gantt accessors ───────────────────────────────────────

static void test_accessors() {
    std::cout << "\n-- Stat & Gantt Accessors --\n";
    sched_init();
    sched_set_algorithm(ALGO_FCFS, 0);

    // P0(0,5) P1(1,3) P2(2,8) → no idle gaps
    sched_create_process(0, 5, 0);
    sched_create_process(1, 3, 0);
    sched_create_process(2, 8, 0);
    sched_run();

    // avg stats: wait=(0+4+6)/3=10/3, turnaround=(5+7+14)/3=26/3
    double avg_wt = -1, avg_tt = -1;
    CHECK("avg_wt returns 0",       sched_get_avg_waiting_time(&avg_wt)    == 0);
    CHECK("avg_tt returns 0",       sched_get_avg_turnaround_time(&avg_tt) == 0);
    CHECK("avg wait ~3.33",         avg_wt > 3.33 && avg_wt < 3.34);
    CHECK("avg turnaround ~8.67",   avg_tt > 8.66 && avg_tt < 8.68);

    // null pointer guards
    CHECK("avg_wt null rejected",   sched_get_avg_waiting_time(nullptr)    == -1);
    CHECK("avg_tt null rejected",   sched_get_avg_turnaround_time(nullptr) == -1);
    CHECK("get_stats null rejected",sched_get_stats(0, nullptr, nullptr)   == -1);
    int dummy_wt = 0, dummy_tt = 0;
    CHECK("get_stats bad pid",      sched_get_stats(99, &dummy_wt, &dummy_tt) == -2);

    // gantt size-query mode (entries=NULL)
    int count = 0;
    CHECK("gantt size query returns 0", sched_get_gantt(nullptr, &count) == 0);
    CHECK("3 gantt entries (no idle)",  count == 3);

    // gantt fill mode
    GanttEntry buf[10];
    int buf_count = 10;
    CHECK("gantt fill returns 0",   sched_get_gantt(buf, &buf_count) == 0);
    CHECK("gantt count written",    buf_count == 3);
    CHECK("entry 0 pid=0",          buf[0].pid == 0);
    CHECK("entry 0 start=0",        buf[0].start_time == 0);
    CHECK("entry 0 end=5",          buf[0].end_time   == 5);
    CHECK("entry 1 pid=1",          buf[1].pid == 1);
    CHECK("entry 1 start=5",        buf[1].start_time == 5);
    CHECK("entry 1 end=8",          buf[1].end_time   == 8);
    CHECK("entry 2 pid=2",          buf[2].pid == 2);
    CHECK("entry 2 start=8",        buf[2].start_time == 8);
    CHECK("entry 2 end=16",         buf[2].end_time   == 16);

    // idle entry shows up when there's a gap
    sched_reset();
    sched_create_process(3, 2, 0); // arrives late → idle 0-3
    sched_run();
    count = 0;
    sched_get_gantt(nullptr, &count);
    CHECK("idle gap produces 2 entries", count == 2);
    buf_count = 10;
    sched_get_gantt(buf, &buf_count);
    CHECK("idle entry pid=-1",      buf[0].pid == -1);
    CHECK("idle entry start=0",     buf[0].start_time == 0);
    CHECK("idle entry end=3",       buf[0].end_time   == 3);

    sched_destroy();
}

// ── Section 3: Scheduling algorithms ─────────────────────────────────────────

static void test_fcfs() {
    std::cout << "\n-- FCFS --\n";
    sched_init();
    sched_set_algorithm(ALGO_FCFS, 0);

    // P0(arr=0,burst=5) P1(arr=1,burst=3) P2(arr=2,burst=8)
    // expected: P0 0-5, P1 5-8, P2 8-16
    sched_create_process(0, 5, 0);
    sched_create_process(1, 3, 0);
    sched_create_process(2, 8, 0);
    CHECK("run returns 0", sched_run() == 0);

    int wt, tt;
    sched_get_stats(0, &wt, &tt);
    CHECK("P0 wait=0",        wt == 0);
    CHECK("P0 turnaround=5",  tt == 5);
    sched_get_stats(1, &wt, &tt);
    CHECK("P1 wait=4",        wt == 4);
    CHECK("P1 turnaround=7",  tt == 7);
    sched_get_stats(2, &wt, &tt);
    CHECK("P2 wait=6",        wt == 6);
    CHECK("P2 turnaround=14", tt == 14);

    // idle gap: P0 arrives at 5, P1 arrives at 0
    sched_reset();
    sched_create_process(5, 3, 0); // pid 0
    sched_create_process(0, 2, 0); // pid 1
    sched_run();
    sched_get_stats(1, &wt, &tt);
    CHECK("idle gap: P1 wait=0",       wt == 0);
    CHECK("idle gap: P1 turnaround=2", tt == 2);
    sched_get_stats(0, &wt, &tt);
    CHECK("idle gap: P0 wait=0",       wt == 0);
    CHECK("idle gap: P0 turnaround=3", tt == 3);

    // simultaneous arrivals: lower pid runs first
    sched_reset();
    sched_create_process(0, 3, 0); // pid 0
    sched_create_process(0, 5, 0); // pid 1
    sched_run();
    sched_get_stats(0, &wt, &tt);
    CHECK("simultaneous: P0 wait=0",       wt == 0);
    sched_get_stats(1, &wt, &tt);
    CHECK("simultaneous: P1 wait=3",       wt == 3);

    sched_destroy();
}

static void test_rr() {
    std::cout << "\n-- Round Robin --\n";
    sched_init();

    // invalid quantum
    CHECK("RR quantum=0 rejected", sched_set_algorithm(ALGO_RR, 0) == -1);
    CHECK("RR quantum=2 accepted", sched_set_algorithm(ALGO_RR, 2) == 0);

    // P0(0,5) P1(0,3) P2(0,8)  quantum=2
    // t=0:  P0 0-2  rem=3
    // t=2:  P1 2-4  rem=1
    // t=4:  P2 4-6  rem=6
    // t=6:  P0 6-8  rem=1
    // t=8:  P1 8-9  rem=0 → finish=9  tt=9  wt=6
    // t=9:  P2 9-11 rem=4
    // t=11: P0 11-12 rem=0 → finish=12 tt=12 wt=7
    // t=12: P2 12-14 rem=2
    // t=14: P2 14-16 rem=0 → finish=16 tt=16 wt=8
    sched_create_process(0, 5, 0); // pid 0
    sched_create_process(0, 3, 0); // pid 1
    sched_create_process(0, 8, 0); // pid 2
    CHECK("run returns 0", sched_run() == 0);

    int wt, tt;
    sched_get_stats(0, &wt, &tt);
    CHECK("RR P0 wait=7",        wt == 7);
    CHECK("RR P0 turnaround=12", tt == 12);
    sched_get_stats(1, &wt, &tt);
    CHECK("RR P1 wait=6",       wt == 6);
    CHECK("RR P1 turnaround=9", tt == 9);
    sched_get_stats(2, &wt, &tt);
    CHECK("RR P2 wait=8",        wt == 8);
    CHECK("RR P2 turnaround=16", tt == 16);

    sched_destroy();
}

static void test_priority() {
    std::cout << "\n-- Priority --\n";
    sched_init();
    sched_set_algorithm(ALGO_PRIORITY, 0);

    // P0(0,5,pri=2) P1(0,3,pri=1) P2(0,8,pri=3)
    // P1 runs first (lowest priority number = highest priority)
    // P1: 0-3  tt=3  wt=0
    // P0: 3-8  tt=8  wt=3
    // P2: 8-16 tt=16 wt=8
    sched_create_process(0, 5, 2); // pid 0
    sched_create_process(0, 3, 1); // pid 1
    sched_create_process(0, 8, 3); // pid 2
    CHECK("run returns 0", sched_run() == 0);

    int wt, tt;
    sched_get_stats(1, &wt, &tt);
    CHECK("Priority P1 wait=0",       wt == 0);
    CHECK("Priority P1 turnaround=3", tt == 3);
    sched_get_stats(0, &wt, &tt);
    CHECK("Priority P0 wait=3",       wt == 3);
    CHECK("Priority P0 turnaround=8", tt == 8);
    sched_get_stats(2, &wt, &tt);
    CHECK("Priority P2 wait=8",        wt == 8);
    CHECK("Priority P2 turnaround=16", tt == 16);

    // edge: no schedulable processes
    sched_reset();
    CHECK("run with no processes returns -1", sched_run() == -1);

    sched_destroy();
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    test_lifecycle();
    test_process_management();
    test_accessors();
    test_fcfs();
    test_rr();
    test_priority();

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed > 0 ? 1 : 0;
}
