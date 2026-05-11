#include "mem.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <limits>

// ── Internal state ────────────────────────────────────────────────────────────

static bool    initialized  = false;
static MemAlgo current_algo = MEM_FIFO;
static int     num_frames   = MEM_DEFAULT_FRAMES;

static std::vector<int>            frames;         // frames[i] = page, -1 = empty
static std::unordered_map<int,int> page_to_frame;  // page -> frame index

static int stat_hits   = 0;
static int stat_faults = 0;

// FIFO: insertion-order list of loaded pages
static std::vector<int> fifo_order;

// LRU: per-page last-access timestamp
static std::unordered_map<int,int> lru_last_used;
static int lru_clock = 0;

// ── Address helpers ───────────────────────────────────────────────────────────

static int page_of(int addr)   { return addr >> 8; }
static int offset_of(int addr) { return addr & (MEM_PAGE_SIZE - 1); }
static int to_physical(int frame, int offset) { return (frame << 8) | offset; }

static int find_free_frame() {
    for (int i = 0; i < num_frames; i++)
        if (frames[i] == -1) return i;
    return -1;
}

static void evict(int page) {
    fifo_order.erase(std::remove(fifo_order.begin(), fifo_order.end(), page),
                     fifo_order.end());
    lru_last_used.erase(page);
    page_to_frame.erase(page);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

int mem_init(void) {
    if (initialized) return -1;
    num_frames   = MEM_DEFAULT_FRAMES;
    current_algo = MEM_FIFO;
    frames.assign(num_frames, -1);
    page_to_frame.clear();
    fifo_order.clear();
    lru_last_used.clear();
    stat_hits = stat_faults = lru_clock = 0;
    initialized = true;
    return 0;
}

int mem_destroy(void) {
    if (!initialized) return -3;
    frames.clear();
    page_to_frame.clear();
    fifo_order.clear();
    lru_last_used.clear();
    initialized = false;
    return 0;
}

int mem_reset(void) {
    if (!initialized) return -3;
    frames.assign(num_frames, -1);
    page_to_frame.clear();
    fifo_order.clear();
    lru_last_used.clear();
    stat_hits = stat_faults = lru_clock = 0;
    // algorithm config preserved
    return 0;
}

// ── Configuration ─────────────────────────────────────────────────────────────

int mem_set_algorithm(MemAlgo algo) {
    if (!initialized) return -3;
    current_algo = algo;
    return 0;
}

// ── Access ────────────────────────────────────────────────────────────────────

int mem_access(int logical_addr) {
    if (!initialized)                                  return -3;
    if (logical_addr < 0 || logical_addr > MEM_MAX_ADDR) return -1;

    int page = page_of(logical_addr);

    if (page_to_frame.count(page)) {
        stat_hits++;
        if (current_algo == MEM_LRU) lru_last_used[page] = ++lru_clock;
        return 0;
    }

    // Page fault
    stat_faults++;
    int f = find_free_frame();

    if (f == -1) {
        int victim = -1;
        if (current_algo == MEM_FIFO) {
            victim = fifo_order.front();
        } else {
            int min_time = std::numeric_limits<int>::max();
            for (auto &kv : page_to_frame) {
                int pg = kv.first;
                if (lru_last_used[pg] < min_time) {
                    min_time = lru_last_used[pg];
                    victim   = pg;
                }
            }
        }
        f = page_to_frame[victim];
        evict(victim);
    }

    frames[f]        = page;
    page_to_frame[page] = f;
    fifo_order.push_back(page);
    if (current_algo == MEM_LRU) lru_last_used[page] = ++lru_clock;

    return 1;
}

// ── Stats & frame inspection ──────────────────────────────────────────────────

int mem_logical_to_physical(int logical_addr, int *physical_addr) {
    if (!initialized)                                  return -3;
    if (!physical_addr || logical_addr < 0
        || logical_addr > MEM_MAX_ADDR)                return -1;

    int page = page_of(logical_addr);
    if (!page_to_frame.count(page)) return -2;

    *physical_addr = to_physical(page_to_frame[page], offset_of(logical_addr));
    return 0;
}

int mem_get_stats(int *hits, int *faults) {
    if (!initialized)      return -3;
    if (!hits || !faults)  return -1;
    *hits   = stat_hits;
    *faults = stat_faults;
    return 0;
}

int mem_get_frames(int *frames_out, int *count) {
    if (!initialized) return -3;
    if (!count)        return -1;
    *count = num_frames;
    if (!frames_out)   return 0;   // size-query mode
    for (int i = 0; i < num_frames; i++) frames_out[i] = frames[i];
    return 0;
}
