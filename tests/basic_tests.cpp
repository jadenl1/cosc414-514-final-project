#include <iostream>
#include "sched.h"
#include "mem.h"
#include "sync.h"

static int passed = 0;
static int failed = 0;

#define CHECK(label, expr) \
    do { \
        if (expr) { std::cout << "[PASS] " << label << "\n"; ++passed; } \
        else       { std::cout << "[FAIL] " << label << "\n"; ++failed; } \
    } while(0)

//life cycle

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

//process management

static void test_process_management() {
    std::cout << "\n-- Process Management --\n";

    sched_init();

    //valid creation
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

//stat and gant accessors

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

//scheduling algorithms

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

// Subsystem B – Memory Management

static void test_mem_lifecycle() {
    std::cout << "\n-- Mem Lifecycle --\n";

    CHECK("mem init succeeds",          mem_init()    == 0);
    CHECK("mem double init fails",      mem_init()    == -1);
    CHECK("mem reset when initialized", mem_reset()   == 0);
    CHECK("mem destroy succeeds",       mem_destroy() == 0);
    CHECK("mem reset when not inited",  mem_reset()   == -3);
    CHECK("mem destroy when not inited",mem_destroy() == -3);
    CHECK("mem reinit after destroy",   mem_init()    == 0);
    mem_destroy();
}

static void test_mem_access_fifo() {
    std::cout << "\n-- Mem Access (FIFO) --\n";
    mem_init();
    mem_set_algorithm(MEM_FIFO);

    //addr is page * 256
    //4 frames available

    CHECK("page 1 first access = fault", mem_access(256)  == 1);
    CHECK("page 1 second access = hit",  mem_access(256)  == 0);
    CHECK("page 2 first access = fault", mem_access(512)  == 1);
    CHECK("page 3 first access = fault", mem_access(768)  == 1);
    CHECK("page 4 first access = fault", mem_access(1024) == 1);

    //frames now full: [p1, p2, p3, p4]  fifo_order=[1,2,3,4]
    CHECK("page 5 fault, evicts page 1", mem_access(1280) == 1);
    CHECK("page 2 still in frames = hit",mem_access(512)  == 0);
    CHECK("page 1 was evicted = fault",  mem_access(256)  == 1);

    //invalid address
    CHECK("negative addr rejected",      mem_access(-1)    == -1);
    CHECK("addr > MAX rejected",         mem_access(99999) == -1);

    //uninit guard
    mem_destroy();
    CHECK("access when not inited",      mem_access(256)   == -3);
}

static void test_mem_access_lru() {
    std::cout << "\n-- Mem Access (LRU) --\n";
    mem_init();
    mem_set_algorithm(MEM_LRU);

    //Load 4 pages to fill frames
    mem_access(256);   //page 1
    mem_access(512);   //page 2
    mem_access(768);   //page 3
    mem_access(1024);  //page 4

    //Touch page 1 again → now MRU: lru_time = {1:5, 2:2, 3:3, 4:4}
    CHECK("page 1 hit (refresh LRU)",    mem_access(256)  == 0);

    //Access page 5 → must evict LRU = page 2 (time=2)
    //lru_times now: {1:5, 3:3, 4:4, 5:6}
    CHECK("page 5 fault, evicts page 2", mem_access(1280) == 1);

    //Access page 2 → fault, evicts page 3 (now LRU with time=3)
    //lru_times now: {1:5, 4:4, 5:6, 2:7}
    CHECK("page 2 fault, evicts page 3", mem_access(512)  == 1);

    //Access page 3 → fault, evicts page 4 (now LRU with time=4)
    //lru_times now: {1:5, 5:6, 2:7, 3:8}
    CHECK("page 3 fault, evicts page 4", mem_access(768)  == 1);

    //Page 1 was refreshed earliest (time=5 when touched) and never evicted
    CHECK("page 1 survived all evictions = hit", mem_access(256) == 0);

    mem_destroy();
}

static void test_mem_stats() {
    std::cout << "\n-- Mem Stats --\n";
    mem_init();

    int h = -1, f = -1;
    CHECK("initial hits=0 faults=0",  mem_get_stats(&h, &f) == 0 && h == 0 && f == 0);

    mem_access(256);  //fault
    mem_access(256);  //hit
    mem_access(512);  //fault
    mem_get_stats(&h, &f);
    CHECK("2 faults recorded",        f == 2);
    CHECK("1 hit recorded",           h == 1);

    //null guards
    CHECK("null hits ptr rejected",   mem_get_stats(nullptr, &f) == -1);
    CHECK("null faults ptr rejected", mem_get_stats(&h, nullptr) == -1);

    //reset stats
    mem_reset();
    mem_get_stats(&h, &f);
    CHECK("reset clears stats",       h == 0 && f == 0);

    mem_destroy();
}

static void test_mem_frames_and_translation() {
    std::cout << "\n-- Mem Frames & Translation --\n";
    mem_init();

    //empty frames
    int frames[MEM_DEFAULT_FRAMES];
    int count = MEM_DEFAULT_FRAMES;
    mem_get_frames(frames, &count);
    CHECK("initial frame count = 4",   count == 4);
    CHECK("frame 0 empty = -1",        frames[0] == -1);
    CHECK("frame 3 empty = -1",        frames[3] == -1);

    //null buffer (size query)
    count = 0;
    CHECK("frames size query returns 0", mem_get_frames(nullptr, &count) == 0);
    CHECK("size query count = 4",        count == 4);

    //Load page 1 into 0
    mem_access(256);  // page 1, offset 0
    mem_get_frames(frames, &(count = MEM_DEFAULT_FRAMES));
    CHECK("frame 0 holds page 1",  frames[0] == 1);
    CHECK("frame 1 still empty",   frames[1] == -1);

    //Translation: page 1 in frame 0
    //logical 256 (p1 offset 0) → physical (0<<8)|0 = 0
    int phys = -1;
    CHECK("translate page 1 offset 0 = 0",   mem_logical_to_physical(256, &phys) == 0 && phys == 0);
    CHECK("translate page 1 offset 1 = 1",   mem_logical_to_physical(257, &phys) == 0 && phys == 1);

    // Page not loaded → -2
    CHECK("translate unloaded page = -2",    mem_logical_to_physical(512, &phys) == -2);

    // Null ptr → -1
    CHECK("translate null ptr = -1",         mem_logical_to_physical(256, nullptr) == -1);

    mem_destroy();
}

// ══════════════════════════════════════════════════════════════════════════════
// Subsystem C – Synchronization
// Note: sem_init is deprecated on macOS; these tests are designed for Ubuntu.
// On macOS, sync_init will fail and tests will be skipped automatically.
// ══════════════════════════════════════════════════════════════════════════════

static bool sync_available = false;

static void test_sync_lifecycle() {
    std::cout << "\n-- Sync Lifecycle --\n";
    SyncState state;

    int rc = sync_init(&state);
    if (rc != 0) {
        std::cout << "  [SKIP] sync_init failed (rc=" << rc
                  << ") — sem_init not supported on this platform\n";
        sync_available = false;
        return;
    }
    sync_available = true;

    CHECK("sync init succeeds",          rc == 0);
    CHECK("sync destroy succeeds",       sync_destroy(&state) == 0);
    CHECK("null state init fails",       sync_init(nullptr)    == -1);
    CHECK("null state destroy fails",    sync_destroy(nullptr) == -1);
}

static void test_sync_permissions() {
    if (!sync_available) { std::cout << "\n-- Sync Permissions (SKIPPED) --\n"; return; }
    std::cout << "\n-- Sync Permissions --\n";

    SyncState state;
    sync_init(&state);

    // USER: read only
    CHECK("USER READ allowed",    sync_check_permission(&state, ROLE_USER,   0, ACTION_READ)   == 0);
    CHECK("USER WRITE denied",    sync_check_permission(&state, ROLE_USER,   0, ACTION_WRITE)  == -1);
    CHECK("USER MANAGE denied",   sync_check_permission(&state, ROLE_USER,   0, ACTION_MANAGE) == -1);

    // EDITOR: read + write
    CHECK("EDITOR READ allowed",  sync_check_permission(&state, ROLE_EDITOR, 0, ACTION_READ)   == 0);
    CHECK("EDITOR WRITE allowed", sync_check_permission(&state, ROLE_EDITOR, 0, ACTION_WRITE)  == 0);
    CHECK("EDITOR MANAGE denied", sync_check_permission(&state, ROLE_EDITOR, 0, ACTION_MANAGE) == -1);

    // ADMIN: all
    CHECK("ADMIN READ allowed",   sync_check_permission(&state, ROLE_ADMIN,  0, ACTION_READ)   == 0);
    CHECK("ADMIN WRITE allowed",  sync_check_permission(&state, ROLE_ADMIN,  0, ACTION_WRITE)  == 0);
    CHECK("ADMIN MANAGE allowed", sync_check_permission(&state, ROLE_ADMIN,  0, ACTION_MANAGE) == 0);

    // invalid inputs
    CHECK("null state = -1",      sync_check_permission(nullptr, ROLE_USER, 0, ACTION_READ)    == -1);
    CHECK("invalid role = -2",    sync_check_permission(&state, ROLE_COUNT, 0, ACTION_READ)    == -2);
    CHECK("invalid action = -2",  sync_check_permission(&state, ROLE_USER,  0, ACTION_COUNT)   == -2);

    sync_destroy(&state);
}

static void test_sync_readers_writers() {
    if (!sync_available) { std::cout << "\n-- Sync Readers-Writers (SKIPPED) --\n"; return; }
    std::cout << "\n-- Sync Readers-Writers --\n";

    SyncState state;
    sync_init(&state);

    // USER can read
    CHECK("begin_read USER ok",      sync_begin_read(&state, ROLE_USER, 0)   == 0);
    CHECK("end_read ok",             sync_end_read(&state)                   == 0);

    // EDITOR can write
    CHECK("begin_write EDITOR ok",   sync_begin_write(&state, ROLE_EDITOR, 0) == 0);
    CHECK("end_write ok",            sync_end_write(&state)                   == 0);

    // USER cannot write (permission denied)
    CHECK("begin_write USER denied", sync_begin_write(&state, ROLE_USER, 0)  == -1);

    // end_read with no active readers
    CHECK("end_read no readers = -3",sync_end_read(&state)                   == -3);

    // end_write with no active writers
    CHECK("end_write no writers = -3",sync_end_write(&state)                 == -3);

    // null state guards
    CHECK("begin_read null = -1",    sync_begin_read(nullptr, ROLE_USER, 0)  == -1);
    CHECK("end_read null = -1",      sync_end_read(nullptr)                  == -1);
    CHECK("begin_write null = -1",   sync_begin_write(nullptr, ROLE_EDITOR, 0) == -1);
    CHECK("end_write null = -1",     sync_end_write(nullptr)                 == -1);

    sync_destroy(&state);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "════ Subsystem A – Scheduling ════";
    test_lifecycle();
    test_process_management();
    test_accessors();
    test_fcfs();
    test_rr();
    test_priority();

    std::cout << "\n════ Subsystem B – Memory ════";
    test_mem_lifecycle();
    test_mem_access_fifo();
    test_mem_access_lru();
    test_mem_stats();
    test_mem_frames_and_translation();

    std::cout << "\n════ Subsystem C – Sync ════";
    test_sync_lifecycle();
    test_sync_permissions();
    test_sync_readers_writers();

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed > 0 ? 1 : 0;
}
