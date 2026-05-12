#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include "sched.h"
#include "mem.h"
#include "sync.h"

// ── Sync global state ─────────────────────────────────────────────────────────

static SyncState g_sync;
static bool      g_sync_ok = false;

// ── Sched output helpers ──────────────────────────────────────────────────────

static void sched_print_gantt() {
    int count = 0;
    if (sched_get_gantt(nullptr, &count) != 0 || count == 0) {
        std::cout << "  (no gantt data — run the scheduler first)\n";
        return;
    }
    GanttEntry *buf = new GanttEntry[count];
    sched_get_gantt(buf, &count);

    std::cout << "\n  ";
    for (int i = 0; i < count; i++) {
        std::string label = (buf[i].pid == -1) ? "IDLE" : "P" + std::to_string(buf[i].pid);
        std::cout << "| " << std::setw(4) << label << " ";
    }
    std::cout << "|\n  ";
    for (int i = 0; i < count; i++)
        std::cout << std::left << std::setw(6) << buf[i].start_time;
    std::cout << buf[count - 1].end_time << "\n\n";
    delete[] buf;
}

static void sched_print_stats() {
    int count = 0;
    sched_get_gantt(nullptr, &count);
    if (count == 0) {
        std::cout << "  (no stats — run the scheduler first)\n";
        return;
    }
    GanttEntry *buf = new GanttEntry[count];
    sched_get_gantt(buf, &count);
    int max_pid = -1;
    for (int i = 0; i < count; i++)
        if (buf[i].pid > max_pid) max_pid = buf[i].pid;
    delete[] buf;

    std::cout << "\n  " << std::left
              << std::setw(6)  << "PID"
              << std::setw(12) << "Wait"
              << std::setw(16) << "Turnaround"
              << "\n  " << std::string(34, '-') << "\n";
    for (int pid = 0; pid <= max_pid; pid++) {
        int wt = 0, tt = 0;
        if (sched_get_stats(pid, &wt, &tt) == 0)
            std::cout << "  " << std::setw(6)  << pid
                               << std::setw(12) << wt
                               << std::setw(16) << tt << "\n";
    }
    double avg_wt = 0, avg_tt = 0;
    sched_get_avg_waiting_time(&avg_wt);
    sched_get_avg_turnaround_time(&avg_tt);
    std::cout << "  " << std::string(34, '-') << "\n"
              << "  Avg wait: "        << std::fixed << std::setprecision(2) << avg_wt
              << "   Avg turnaround: " << avg_tt << "\n\n";
}

// ── Mem output helpers ────────────────────────────────────────────────────────

static void mem_print_frames() {
    int buf[MEM_DEFAULT_FRAMES];
    int count = MEM_DEFAULT_FRAMES;
    mem_get_frames(buf, &count);
    std::cout << "  Frames: [";
    for (int i = 0; i < count; i++) {
        if (buf[i] == -1) std::cout << " -- ";
        else              std::cout << std::setw(3) << buf[i] << " ";
    }
    std::cout << "]\n";
}

static void mem_print_stats() {
    int h = 0, f = 0;
    mem_get_stats(&h, &f);
    int total = h + f;
    std::cout << "  Hits: " << h << "  Faults: " << f;
    if (total > 0)
        std::cout << "  Hit rate: " << std::fixed << std::setprecision(1)
                  << (100.0 * h / total) << "%";
    std::cout << "\n";
}

static void mem_run_demo() {
    mem_reset();
    int pages[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int n = (int)(sizeof(pages) / sizeof(pages[0]));
    std::cout << "\n  Demo sequence: ";
    for (int i = 0; i < n; i++) std::cout << pages[i] << " ";
    std::cout << "\n\n";
    for (int i = 0; i < n; i++) {
        int addr = pages[i] * MEM_PAGE_SIZE;
        int rc   = mem_access(addr);
        int phys = -1;
        mem_logical_to_physical(addr, &phys);
        std::cout << "  " << (rc == 0 ? "HIT  " : "FAULT")
                  << "  page=" << pages[i]
                  << "  addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr
                  << std::dec << std::setfill(' ');
        if (phys >= 0)
            std::cout << "  phys=0x" << std::hex << phys << std::dec;
        std::cout << "\n";
        mem_print_frames();
    }
    std::cout << "\n";
    mem_print_stats();
}

// ── Sync helpers ──────────────────────────────────────────────────────────────

static sync_role_t parse_role(const std::string &s) {
    if (s == "USER")   return ROLE_USER;
    if (s == "EDITOR") return ROLE_EDITOR;
    if (s == "ADMIN")  return ROLE_ADMIN;
    return ROLE_COUNT;
}

static sync_action_t parse_action(const std::string &s) {
    if (s == "READ")   return ACTION_READ;
    if (s == "WRITE")  return ACTION_WRITE;
    if (s == "MANAGE") return ACTION_MANAGE;
    return ACTION_COUNT;
}

static const char *role_name(sync_role_t r) {
    switch (r) {
        case ROLE_USER:   return "USER";
        case ROLE_EDITOR: return "EDITOR";
        case ROLE_ADMIN:  return "ADMIN";
        default:          return "UNKNOWN";
    }
}

static const char *action_name(sync_action_t a) {
    switch (a) {
        case ACTION_READ:   return "READ";
        case ACTION_WRITE:  return "WRITE";
        case ACTION_MANAGE: return "MANAGE";
        default:            return "UNKNOWN";
    }
}

static void sync_print_permissions() {
    sync_role_t   roles[]   = { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN };
    sync_action_t actions[] = { ACTION_READ, ACTION_WRITE, ACTION_MANAGE };
    std::cout << "  Role      READ    WRITE   MANAGE\n"
              << "  " << std::string(36, '-') << "\n";
    for (auto role : roles) {
        std::cout << "  " << std::left;
        std::cout.width(10); std::cout << role_name(role);
        for (auto action : actions) {
            int r = sync_check_permission(&g_sync, role, 0, action);
            std::cout.width(8); std::cout << (r == 0 ? "YES" : "no");
        }
        std::cout << "\n";
    }
}

static void sync_run_demo() {
    std::cout << "\n  Running preset scenarios...\n";

    SyncState s;

    std::cout << "\n  -- Scenario 1: USER read --\n";
    sync_init(&s);
    int rc = sync_begin_read(&s, ROLE_USER, 0);
    std::cout << "    begin_read (USER):  " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    if (rc == 0) {
        rc = sync_end_read(&s);
        std::cout << "    end_read:          " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    }
    sync_destroy(&s);

    std::cout << "\n  -- Scenario 2: EDITOR write --\n";
    sync_init(&s);
    rc = sync_begin_write(&s, ROLE_EDITOR, 0);
    std::cout << "    begin_write (EDITOR): " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    if (rc == 0) {
        rc = sync_end_write(&s);
        std::cout << "    end_write:            " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
    }
    sync_destroy(&s);

    std::cout << "\n  -- Scenario 3: USER write (permission denied) --\n";
    sync_init(&s);
    rc = sync_begin_write(&s, ROLE_USER, 0);
    std::cout << "    begin_write (USER): " << (rc < 0 ? "DENIED" : "OK") << " (rc=" << rc << ")\n";
    sync_destroy(&s);

    std::cout << "\n  -- Scenario 4: Permission matrix --\n";
    sync_init(&s);
    sync_role_t   roles[]   = { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN };
    sync_action_t actions[] = { ACTION_READ, ACTION_WRITE, ACTION_MANAGE };
    std::cout << "    Role      READ    WRITE   MANAGE\n"
              << "    " << std::string(36, '-') << "\n";
    for (auto role : roles) {
        std::cout << "    " << std::left;
        std::cout.width(10); std::cout << role_name(role);
        for (auto action : actions) {
            int r = sync_check_permission(&s, role, 0, action);
            std::cout.width(8); std::cout << (r == 0 ? "YES" : "no");
        }
        std::cout << "\n";
    }
    sync_destroy(&s);

    std::cout << "\n  Demo complete.\n";
}

// ── Subsystem handlers ────────────────────────────────────────────────────────

static void handle_sched(std::istringstream &ss) {
    std::string cmd;
    ss >> cmd;
    if (cmd.empty()) {
        std::cout << "  usage: sched <command> — type 'sched help' for commands\n";
        return;
    }

    if (cmd == "help") {
        std::cout <<
            "\n  sched commands:\n"
            "    sched create_process <arrival> <burst> <priority>\n"
            "    sched schedule <FCFS|RR|PRIORITY> [quantum]\n"
            "    sched run\n"
            "    sched gantt\n"
            "    sched stats\n"
            "    sched reset\n\n";

    } else if (cmd == "create_process") {
        int arrival, burst, priority;
        if (!(ss >> arrival >> burst >> priority)) {
            std::cout << "  usage: sched create_process <arrival> <burst> <priority>\n";
            return;
        }
        int pid = sched_create_process(arrival, burst, priority);
        if (pid < 0)
            std::cout << "  error: invalid arguments (code " << pid << ")\n";
        else
            std::cout << "  created process P" << pid << "\n";

    } else if (cmd == "schedule") {
        std::string algo_str;
        int quantum = 0;
        if (!(ss >> algo_str)) {
            std::cout << "  usage: sched schedule <FCFS|RR|PRIORITY> [quantum]\n";
            return;
        }
        SchedAlgo algo;
        if      (algo_str == "FCFS")     algo = ALGO_FCFS;
        else if (algo_str == "RR")       { algo = ALGO_RR; ss >> quantum; }
        else if (algo_str == "PRIORITY") algo = ALGO_PRIORITY;
        else { std::cout << "  unknown algorithm: " << algo_str << "\n"; return; }

        int rc = sched_set_algorithm(algo, quantum);
        if (rc != 0)
            std::cout << "  error: " << (algo == ALGO_RR ? "quantum must be > 0" : "invalid") << "\n";
        else
            std::cout << "  algorithm set to " << algo_str
                      << (algo == ALGO_RR ? " (quantum=" + std::to_string(quantum) + ")" : "") << "\n";

    } else if (cmd == "run") {
        int rc = sched_run();
        if      (rc == -1) std::cout << "  error: no processes to schedule\n";
        else if (rc != 0)  std::cout << "  error: run failed (code " << rc << ")\n";
        else { std::cout << "  simulation complete\n"; sched_print_gantt(); sched_print_stats(); }

    } else if (cmd == "gantt") {
        sched_print_gantt();
    } else if (cmd == "stats") {
        sched_print_stats();
    } else if (cmd == "reset") {
        sched_reset();
        std::cout << "  processes cleared\n";
    } else {
        std::cout << "  unknown sched command: " << cmd << " (type 'sched help')\n";
    }
}

static void handle_mem(std::istringstream &ss) {
    std::string cmd;
    ss >> cmd;
    if (cmd.empty()) {
        std::cout << "  usage: mem <command> — type 'mem help' for commands\n";
        return;
    }

    if (cmd == "help") {
        std::cout <<
            "\n  mem commands:\n"
            "    mem algorithm <FIFO|LRU>\n"
            "    mem access <address>\n"
            "    mem frames\n"
            "    mem stats\n"
            "    mem reset\n"
            "    mem demo\n\n";

    } else if (cmd == "algorithm") {
        std::string algo_str;
        if (!(ss >> algo_str)) {
            std::cout << "  usage: mem algorithm <FIFO|LRU>\n"; return;
        }
        MemAlgo algo;
        if      (algo_str == "FIFO") algo = MEM_FIFO;
        else if (algo_str == "LRU")  algo = MEM_LRU;
        else { std::cout << "  unknown algorithm: " << algo_str << "\n"; return; }
        mem_set_algorithm(algo);
        std::cout << "  algorithm set to " << algo_str << "\n";

    } else if (cmd == "access") {
        int addr;
        if (!(ss >> addr)) {
            std::cout << "  usage: mem access <address>\n"; return;
        }
        int rc = mem_access(addr);
        if (rc == -3) { std::cout << "  error: not initialized\n";  return; }
        if (rc == -1) { std::cout << "  error: invalid address\n";  return; }
        int phys = -1;
        mem_logical_to_physical(addr, &phys);
        std::cout << "  " << (rc == 0 ? "HIT  " : "FAULT")
                  << "  page=" << (addr >> 8)
                  << "  offset=" << (addr & 0xFF);
        if (phys >= 0)
            std::cout << "  phys=0x" << std::hex << phys << std::dec;
        std::cout << "\n";
        mem_print_frames();

    } else if (cmd == "frames") {
        mem_print_frames();
    } else if (cmd == "stats") {
        mem_print_stats();
    } else if (cmd == "reset") {
        mem_reset();
        std::cout << "  frames and stats cleared\n";
    } else if (cmd == "demo") {
        mem_run_demo();
    } else {
        std::cout << "  unknown mem command: " << cmd << " (type 'mem help')\n";
    }
}

static void handle_sync(std::istringstream &ss) {
    if (!g_sync_ok) {
        std::cout << "  error: sync unavailable — sem_init not supported on this platform\n";
        return;
    }

    std::string cmd;
    ss >> cmd;
    if (cmd.empty()) {
        std::cout << "  usage: sync <command> — type 'sync help' for commands\n";
        return;
    }

    if (cmd == "help") {
        std::cout <<
            "\n  sync commands:\n"
            "    sync read   <USER|EDITOR|ADMIN>\n"
            "    sync write  <USER|EDITOR|ADMIN>\n"
            "    sync check  <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>\n"
            "    sync permissions\n"
            "    sync status\n"
            "    sync reset\n"
            "    sync demo\n\n";

    } else if (cmd == "read") {
        std::string r;
        if (!(ss >> r)) { std::cout << "  usage: sync read <USER|EDITOR|ADMIN>\n"; return; }
        sync_role_t role = parse_role(r);
        if (role == ROLE_COUNT) { std::cout << "  unknown role: " << r << "\n"; return; }
        int rc = sync_begin_read(&g_sync, role, 0);
        if (rc < 0) {
            std::cout << "  begin_read (" << role_name(role) << "): DENIED (rc=" << rc << ")\n";
        } else {
            std::cout << "  begin_read (" << role_name(role) << "): OK\n";
            rc = sync_end_read(&g_sync);
            std::cout << "  end_read: " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
        }

    } else if (cmd == "write") {
        std::string r;
        if (!(ss >> r)) { std::cout << "  usage: sync write <USER|EDITOR|ADMIN>\n"; return; }
        sync_role_t role = parse_role(r);
        if (role == ROLE_COUNT) { std::cout << "  unknown role: " << r << "\n"; return; }
        int rc = sync_begin_write(&g_sync, role, 0);
        if (rc < 0) {
            std::cout << "  begin_write (" << role_name(role) << "): DENIED (rc=" << rc << ")\n";
        } else {
            std::cout << "  begin_write (" << role_name(role) << "): OK\n";
            rc = sync_end_write(&g_sync);
            std::cout << "  end_write: " << (rc == 0 ? "OK" : "FAIL") << " (rc=" << rc << ")\n";
        }

    } else if (cmd == "check") {
        std::string r, a;
        if (!(ss >> r >> a)) {
            std::cout << "  usage: sync check <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>\n"; return;
        }
        sync_role_t   role   = parse_role(r);
        sync_action_t action = parse_action(a);
        if (role   == ROLE_COUNT)   { std::cout << "  unknown role: "   << r << "\n"; return; }
        if (action == ACTION_COUNT) { std::cout << "  unknown action: " << a << "\n"; return; }
        int rc = sync_check_permission(&g_sync, role, 0, action);
        std::cout << "  " << role_name(role) << " " << action_name(action)
                  << ": " << (rc == 0 ? "ALLOWED" : "DENIED") << "\n";

    } else if (cmd == "permissions") {
        sync_print_permissions();

    } else if (cmd == "status") {
        std::cout << "  Active readers:  " << g_sync.active_readers  << "\n"
                  << "  Active writers:  " << g_sync.active_writers  << "\n"
                  << "  Waiting writers: " << g_sync.waiting_writers << "\n";

    } else if (cmd == "reset") {
        sync_destroy(&g_sync);
        int rc = sync_init(&g_sync);
        g_sync_ok = (rc == 0);
        std::cout << (g_sync_ok ? "  sync state reset\n" : "  error: reinit failed\n");

    } else if (cmd == "demo") {
        sync_run_demo();

    } else {
        std::cout << "  unknown sync command: " << cmd << " (type 'sync help')\n";
    }
}

// ── Vertical slice demo ───────────────────────────────────────────────────────

static void run_vertical_slice() {
    std::cout <<
        "\n════════════════════════════════════════════\n"
        "  MOSS Vertical Slice Demo\n"
        "  One process flowing through all subsystems\n"
        "════════════════════════════════════════════\n";

    // ── Step 1: Process creation ──────────────────────────────────────────────
    std::cout <<
        "\n── Step 1: Process Creation (Subsystem A) ──\n"
        "  Creating two processes with FCFS scheduling.\n\n";

    sched_reset();
    mem_reset();
    sched_set_algorithm(ALGO_FCFS, 0);

    int p0 = sched_create_process(0, 5, 1);
    int p1 = sched_create_process(1, 3, 2);
    std::cout << "  created process P" << p0 << " (arrival=0, burst=5, priority=1)\n";
    std::cout << "  created process P" << p1 << " (arrival=1, burst=3, priority=2)\n";

    // ── Step 2: CPU scheduling ────────────────────────────────────────────────
    std::cout <<
        "\n── Step 2: CPU Scheduling (Subsystem A) ────\n"
        "  Running FCFS scheduler on the process table.\n";

    sched_run();
    sched_print_gantt();
    sched_print_stats();

    // ── Step 3: Memory access ─────────────────────────────────────────────────
    std::cout <<
        "── Step 3: Memory Access (Subsystem B) ─────\n"
        "  P0 accesses logical address 0x0100 (page 1, offset 0).\n\n";

    int addr = 0x0100;
    int rc   = mem_access(addr);
    int phys = -1;
    mem_logical_to_physical(addr, &phys);
    std::cout << "  access 0x0100 → " << (rc == 1 ? "PAGE FAULT" : "HIT")
              << "  phys=0x" << std::hex << phys << std::dec << "\n";
    mem_print_frames();

    std::cout << "\n  P0 accesses 0x0100 again (should hit the cache).\n\n";
    rc = mem_access(addr);
    std::cout << "  access 0x0100 → " << (rc == 0 ? "HIT" : "PAGE FAULT") << "\n";
    mem_print_frames();

    int h = 0, f = 0;
    mem_get_stats(&h, &f);
    std::cout << "\n  Memory stats: hits=" << h << "  faults=" << f
              << "  hit rate=" << std::fixed << std::setprecision(1)
              << (100.0 * h / (h + f)) << "%\n";

    // ── Step 4: Synchronization / protection check ────────────────────────────
    std::cout <<
        "\n── Step 4: Synchronization & Protection (Subsystem C) ──\n";

    if (!g_sync_ok) {
        std::cout << "  [SKIP] sync unavailable on this platform\n";
    } else {
        std::cout << "  Checking write permission before P0 updates shared data.\n\n";

        sync_role_t   roles[]   = { ROLE_USER, ROLE_EDITOR, ROLE_ADMIN };
        sync_action_t actions[] = { ACTION_READ, ACTION_WRITE, ACTION_MANAGE };
        std::cout << "  Role      READ    WRITE   MANAGE\n"
                  << "  " << std::string(36, '-') << "\n";
        for (auto role : roles) {
            std::cout << "  " << std::left;
            std::cout.width(10); std::cout << role_name(role);
            for (auto action : actions) {
                int r = sync_check_permission(&g_sync, role, 0, action);
                std::cout.width(8); std::cout << (r == 0 ? "YES" : "no");
            }
            std::cout << "\n";
        }

        std::cout << "\n  EDITOR acquires write lock to update shared resource:\n";
        rc = sync_begin_write(&g_sync, ROLE_EDITOR, 0);
        std::cout << "    begin_write (EDITOR): " << (rc == 0 ? "OK" : "FAIL") << "\n";
        if (rc == 0) {
            rc = sync_end_write(&g_sync);
            std::cout << "    end_write:            " << (rc == 0 ? "OK" : "FAIL") << "\n";
        }

        std::cout << "\n  USER attempts write (should be denied by permission matrix):\n";
        rc = sync_begin_write(&g_sync, ROLE_USER, 0);
        std::cout << "    begin_write (USER):   " << (rc < 0 ? "DENIED" : "OK")
                  << " (rc=" << rc << ")\n";
    }

    std::cout <<
        "\n════════════════════════════════════════════\n"
        "  Vertical slice complete.\n"
        "  All four subsystem integration points shown:\n"
        "    [A] Process created and scheduled (FCFS)\n"
        "    [B] Memory accessed — page fault then hit\n"
        "    [C] Permission check + write lock acquired\n"
        "════════════════════════════════════════════\n\n";
}

// ── Top-level help ────────────────────────────────────────────────────────────

static void print_help() {
    std::cout <<
        "\n  MOSS – Mini Operating System Services Simulator\n\n"
        "  Subsystem A – Scheduling:\n"
        "    sched create_process <arrival> <burst> <priority>\n"
        "    sched schedule <FCFS|RR|PRIORITY> [quantum]\n"
        "    sched run | gantt | stats | reset\n\n"
        "  Subsystem B – Memory:\n"
        "    mem algorithm <FIFO|LRU>\n"
        "    mem access <address> | frames | stats | reset | demo\n\n"
        "  Subsystem C – Sync:\n"
        "    sync read | write <USER|EDITOR|ADMIN>\n"
        "    sync check <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>\n"
        "    sync permissions | status | reset | demo\n\n"
        "  Type 'sched help', 'mem help', or 'sync help' for detailed usage.\n"
        "  demo    run vertical slice (end-to-end integration demo)\n"
        "  exit    quit\n\n";
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    sched_init();
    mem_init();
    g_sync_ok = (sync_init(&g_sync) == 0);

    std::cout << "MOSS – Mini Operating System Services Simulator\n"
              << "Type 'help' for commands\n";
    if (!g_sync_ok)
        std::cout << "  [note] sync unavailable on this platform\n";
    std::cout << "\n";

    std::string line;
    while (true) {
        std::cout << "moss> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token.empty()) continue;

        if      (token == "exit" || token == "quit") break;
        else if (token == "help")  print_help();
        else if (token == "demo")  run_vertical_slice();
        else if (token == "sched") handle_sched(ss);
        else if (token == "mem")   handle_mem(ss);
        else if (token == "sync")  handle_sync(ss);
        else std::cout << "  unknown command: " << token << " (type 'help')\n";
    }

    sched_destroy();
    mem_destroy();
    if (g_sync_ok) sync_destroy(&g_sync);
    return 0;
}
