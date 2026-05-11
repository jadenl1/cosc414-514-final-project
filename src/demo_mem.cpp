#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include "mem.h"

// ── Output helpers ────────────────────────────────────────────────────────────

static void print_frames() {
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

static void print_stats() {
    int h = 0, f = 0;
    mem_get_stats(&h, &f);
    int total = h + f;
    std::cout << "  Hits: " << h << "  Faults: " << f;
    if (total > 0)
        std::cout << "  Hit rate: " << std::fixed << std::setprecision(1)
                  << (100.0 * h / total) << "%";
    std::cout << "\n";
}

static void print_help() {
    std::cout <<
        "\n  Commands:\n"
        "    algorithm <FIFO|LRU>         set page replacement algorithm\n"
        "    access <address>             access logical address (0-65535)\n"
        "    frames                       show current frame contents\n"
        "    stats                        show hit/fault statistics\n"
        "    reset                        clear frames and stats\n"
        "    demo                         run a preset page reference sequence\n"
        "    help\n"
        "    exit\n\n";
}

static void run_demo() {
    mem_reset();
    int pages[] = {1,2,3,4,1,2,5,1,2,3,4,5};
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
        print_frames();
    }
    std::cout << "\n";
    print_stats();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

int main() {
    mem_init();
    std::cout << "MOSS Memory — " << MEM_DEFAULT_FRAMES << " frames | "
              << MEM_PAGE_SIZE << "-byte pages | 16-bit addresses\n"
              << "Type 'help' for commands\n";

    std::string line;
    while (true) {
        std::cout << "mem> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "exit" || cmd == "quit") {
            break;

        } else if (cmd == "help") {
            print_help();

        } else if (cmd == "algorithm") {
            std::string algo_str;
            if (!(ss >> algo_str)) {
                std::cout << "  usage: algorithm <FIFO|LRU>\n"; continue;
            }
            MemAlgo algo;
            if      (algo_str == "FIFO") algo = MEM_FIFO;
            else if (algo_str == "LRU")  algo = MEM_LRU;
            else { std::cout << "  unknown algorithm: " << algo_str << "\n"; continue; }
            mem_set_algorithm(algo);
            std::cout << "  algorithm set to " << algo_str << "\n";

        } else if (cmd == "access") {
            int addr;
            if (!(ss >> addr)) {
                std::cout << "  usage: access <address>\n"; continue;
            }
            int rc = mem_access(addr);
            if (rc == -3) { std::cout << "  error: not initialized\n";  continue; }
            if (rc == -1) { std::cout << "  error: invalid address\n";  continue; }
            int phys = -1;
            mem_logical_to_physical(addr, &phys);
            std::cout << "  " << (rc == 0 ? "HIT  " : "FAULT")
                      << "  page=" << (addr >> 8)
                      << "  offset=" << (addr & 0xFF);
            if (phys >= 0)
                std::cout << "  phys=0x" << std::hex << phys << std::dec;
            std::cout << "\n";
            print_frames();

        } else if (cmd == "frames") {
            print_frames();

        } else if (cmd == "stats") {
            print_stats();

        } else if (cmd == "reset") {
            mem_reset();
            std::cout << "  frames and stats cleared\n";

        } else if (cmd == "demo") {
            run_demo();

        } else {
            std::cout << "  unknown command: " << cmd << " (type 'help')\n";
        }
    }

    mem_destroy();
    return 0;
}
