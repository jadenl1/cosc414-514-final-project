#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include "sched.h"

// ── Output helpers ────────────────────────────────────────────────────────────

static void print_gantt() {
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

    for (int i = 0; i < count; i++) {
        std::cout << std::left << std::setw(6) << buf[i].start_time;
    }
    std::cout << buf[count - 1].end_time << "\n\n";

    delete[] buf;
}

static void print_stats() {
    int count = 0;
    sched_get_gantt(nullptr, &count);
    if (count == 0) {
        std::cout << "  (no stats — run the scheduler first)\n";
        return;
    }

    // find highest pid by scanning gantt
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
              << "  Avg wait: "       << std::fixed << std::setprecision(2) << avg_wt
              << "   Avg turnaround: " << avg_tt << "\n\n";
}

static void print_help() {
    std::cout <<
        "\n  Commands:\n"
        "    create_process <arrival> <burst> <priority>\n"
        "    schedule <FCFS|RR|PRIORITY> [quantum]   (quantum required for RR)\n"
        "    run                                      run the simulation\n"
        "    gantt                                    print Gantt chart\n"
        "    stats                                    print per-process stats\n"
        "    reset                                    clear processes, keep algorithm\n"
        "    help\n"
        "    exit\n\n";
}

// ── Main loop ─────────────────────────────────────────────────────────────────

int main() {
    sched_init();
    std::cout << "MOSS Scheduler — type 'help' for commands\n";

    std::string line;
    while (true) {
        std::cout << "moss> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd.empty()) continue;

        if (cmd == "exit" || cmd == "quit") {
            break;

        } else if (cmd == "help") {
            print_help();

        } else if (cmd == "create_process") {
            int arrival, burst, priority;
            if (!(ss >> arrival >> burst >> priority)) {
                std::cout << "  usage: create_process <arrival> <burst> <priority>\n";
                continue;
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
                std::cout << "  usage: schedule <FCFS|RR|PRIORITY> [quantum]\n";
                continue;
            }
            SchedAlgo algo;
            if      (algo_str == "FCFS")     algo = ALGO_FCFS;
            else if (algo_str == "RR")       { algo = ALGO_RR; ss >> quantum; }
            else if (algo_str == "PRIORITY") algo = ALGO_PRIORITY;
            else { std::cout << "  unknown algorithm: " << algo_str << "\n"; continue; }

            int rc = sched_set_algorithm(algo, quantum);
            if (rc != 0)
                std::cout << "  error: " << (algo == ALGO_RR ? "quantum must be > 0" : "invalid") << "\n";
            else
                std::cout << "  algorithm set to " << algo_str
                          << (algo == ALGO_RR ? " (quantum=" + std::to_string(quantum) + ")" : "") << "\n";

        } else if (cmd == "run") {
            int rc = sched_run();
            if (rc == -1) std::cout << "  error: no processes to schedule\n";
            else if (rc != 0) std::cout << "  error: run failed (code " << rc << ")\n";
            else { std::cout << "  simulation complete\n"; print_gantt(); print_stats(); }

        } else if (cmd == "gantt") {
            print_gantt();

        } else if (cmd == "stats") {
            print_stats();

        } else if (cmd == "reset") {
            sched_reset();
            std::cout << "  processes cleared\n";

        } else {
            std::cout << "  unknown command: " << cmd << " (type 'help')\n";
        }
    }

    sched_destroy();
    return 0;
}
