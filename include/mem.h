#ifndef MEM_H
#define MEM_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Configuration ─────────────────────────────────────────────────────────────

#define MEM_DEFAULT_FRAMES  4
#define MEM_PAGE_SIZE       256          // bytes per page (8-bit offset)
#define MEM_ADDR_BITS       16           // logical address width
#define MEM_MAX_ADDR        ((1 << MEM_ADDR_BITS) - 1)   // 65535

// ── Enums ─────────────────────────────────────────────────────────────────────

typedef enum {
    MEM_FIFO,
    MEM_LRU
} MemAlgo;

// ── API ───────────────────────────────────────────────────────────────────────
//
// Return codes: 0 = success, negative = error
//   -1  generic error / invalid argument
//   -2  not found
//   -3  subsystem not initialized

// Lifecycle
int mem_init(void);
int mem_destroy(void);
int mem_reset(void);    // clears frames and stats; keeps algorithm

// Configuration
int mem_set_algorithm(MemAlgo algo);

// Access
// Returns: 0 = hit, 1 = page fault, -1 = invalid address, -3 = not initialized
int mem_access(int logical_addr);

// Address translation — returns -1 if page is not currently loaded
int mem_logical_to_physical(int logical_addr, int *physical_addr);

// Stats — valid any time after mem_init()
int mem_get_stats(int *hits, int *faults);

// Frame inspection — pass frames_out=NULL to query count only
int mem_get_frames(int *frames_out, int *count);   // -1 in array = empty frame

#ifdef __cplusplus
}
#endif

#endif /* MEM_H */
