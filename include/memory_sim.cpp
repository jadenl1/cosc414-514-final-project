/*
 * Memory Management & Virtual Memory Simulation (Simplified)
 * -----------------------------------------------------------
 * - 16-bit logical addresses, 8-bit page offset
 * - 256-byte pages, 4 physical frames
 * - FIFO and LRU page replacement
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <limits>

// -- Configuration ------------------------------------------
static const int OFFSET_BITS = 8;
static const int PAGE_SIZE   = 1 << OFFSET_BITS;   // 256
static const int NUM_FRAMES  = 4;
static const int MAX_ADDR    = (1 << 16) - 1;       // 65535

// -- Address helpers -----------------------------------------
int pageOf(int addr)   { return addr >> OFFSET_BITS; }
int offsetOf(int addr) { return addr & (PAGE_SIZE - 1); }
int physical(int frame, int offset) { return (frame << OFFSET_BITS) | offset; }

// -- Shared simulator state ----------------------------------
struct Sim {
    std::vector<int>          frames;       // frame[i] = page loaded (-1 = empty)
    std::unordered_map<int,int> pageFrame;  // page -> frame index
    int faults = 0, hits = 0;

    Sim() : frames(NUM_FRAMES, -1) {}

    // Find a free frame, or return -1 if all are full
    int freeFrame() {
        for (int i = 0; i < NUM_FRAMES; ++i)
            if (frames[i] == -1) return i;
        return -1;
    }

    // Load a page into a frame (evicting whatever was there)
    void load(int page, int frame, int evictedPage = -1) {
        if (evictedPage != -1) {
            pageFrame.erase(evictedPage);
            std::cout << "    Evict page " << evictedPage << " from frame " << frame << "\n";
        }
        frames[frame] = page;
        pageFrame[page] = frame;
    }

    void printFrames() const {
        std::cout << "    Frames: [";
        for (int i = 0; i < NUM_FRAMES; ++i) {
            if (frames[i] == -1) std::cout << " -- ";
            else std::cout << std::setw(3) << frames[i] << " ";
        }
        std::cout << "]\n";
    }

    void printSummary(const std::string& name) const {
        int total = faults + hits;
        std::cout << "\n-- " << name << " Summary --------------------\n"
                  << "  Faults : " << faults << "\n"
                  << "  Hits   : " << hits   << "\n";
        if (total > 0)
            std::cout << "  Hit %  : " << std::fixed << std::setprecision(1)
                      << (100.0 * hits / total) << "%\n";
        std::cout << std::string(40, '-') << "\n";
    }
};

// -- FIFO ----------------------------------------------------
void fifoAccess(Sim& s, std::vector<int>& order, int addr) {
    int page = pageOf(addr), off = offsetOf(addr);

    if (s.pageFrame.count(page)) {
        ++s.hits;
        std::cout << "  HIT   page=" << page
                  << " -> phys=0x" << std::hex << physical(s.pageFrame[page], off)
                  << std::dec << "\n";
    } else {
        ++s.faults;
        int f = s.freeFrame();
        if (f == -1) {
            // evict oldest
            int evict = order.front();
            order.erase(order.begin());
            f = s.pageFrame[evict];
            s.load(page, f, evict);
        } else {
            s.load(page, f);
        }
        order.push_back(page);
        std::cout << "  FAULT page=" << page
                  << " -> frame=" << f
                  << " phys=0x" << std::hex << physical(f, off)
                  << std::dec << "\n";
    }
    s.printFrames();
}

// -- LRU -----------------------------------------------------
void lruAccess(Sim& s, std::unordered_map<int,int>& lastUsed, int& clock, int addr) {
    int page = pageOf(addr), off = offsetOf(addr);
    ++clock;

    if (s.pageFrame.count(page)) {
        ++s.hits;
        lastUsed[page] = clock;
        std::cout << "  HIT   page=" << page
                  << " -> phys=0x" << std::hex << physical(s.pageFrame[page], off)
                  << std::dec << "\n";
    } else {
        ++s.faults;
        int f = s.freeFrame();
        if (f == -1) {
            // evict least recently used
            int lruPage = -1, lruTime = std::numeric_limits<int>::max();
            for (auto& [pg, fr] : s.pageFrame)
                if (lastUsed[pg] < lruTime) { lruTime = lastUsed[pg]; lruPage = pg; }
            f = s.pageFrame[lruPage];
            lastUsed.erase(lruPage);
            s.load(page, f, lruPage);
        } else {
            s.load(page, f);
        }
        lastUsed[page] = clock;
        std::cout << "  FAULT page=" << page
                  << " -> frame=" << f
                  << " phys=0x" << std::hex << physical(f, off)
                  << std::dec << "\n";
    }
    s.printFrames();
}

// -- Demo ----------------------------------------------------
void runDemo() {
    std::vector<int> pages = {1,2,3,4,1,2,5,1,2,3,4,5};
    std::cout << "\nDemo pages: ";
    for (int p : pages) std::cout << p << " ";
    std::cout << "\n";

    // FIFO
    std::cout << "\n-- FIFO ------------------------------------------\n";
    { Sim s; std::vector<int> order;
      for (int p : pages) fifoAccess(s, order, p * PAGE_SIZE);
      s.printSummary("FIFO"); }

    // LRU
    std::cout << "\n-- LRU -------------------------------------------\n";
    { Sim s; std::unordered_map<int,int> lastUsed; int clock = 0;
      for (int p : pages) lruAccess(s, lastUsed, clock, p * PAGE_SIZE);
      s.printSummary("LRU"); }
}

// -- Main ----------------------------------------------------
int main() {
    std::cout << "Memory Simulator | " << NUM_FRAMES
              << " frames | " << PAGE_SIZE << "-byte pages\n"
              << std::string(45, '=') << "\n";

    int choice;
    while (true) {
        std::cout << "\n1) FIFO  2) LRU  3) Demo  0) Quit\n> ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (choice == 0) break;
        if (choice == 3) { runDemo(); continue; }
        if (choice != 1 && choice != 2) { std::cout << "Bad choice.\n"; continue; }

        int n;
        std::cout << "How many accesses? ";
        std::cin >> n;

        Sim s;
        std::vector<int> order;           // FIFO only
        std::unordered_map<int,int> lu;   // LRU only
        int clock = 0;

        std::cout << "\n-- " << (choice == 1 ? "FIFO" : "LRU") << " --\n";
        for (int i = 0; i < n; ++i) {
            int addr;
            std::cout << "  Address (0-" << MAX_ADDR << "): ";
            std::cin >> addr;
            if (choice == 1) fifoAccess(s, order, addr);
            else             lruAccess(s, lu, clock, addr);
        }
        s.printSummary(choice == 1 ? "FIFO" : "LRU");
    }
    return 0;
}
