# MOSS Vertical Slice

**Course:** COSC 414/514 – Operating Systems **Team:** Jaden Leonard, Amir Taylor, Fletcher Baccus, Uzoma Bruce

---

## Overview

The vertical slice demonstrates one process flowing end-to-end through all three MOSS subsystems in a single session of the unified `./moss` CLI. It is triggered by typing `demo` at the `moss>` prompt.

---

## How to Run

```bash
make
./moss
moss> demo
```

---

## Subsystem Interaction Walkthrough

### Step 1 — Process Creation (Subsystem A)

Two processes are created in the scheduler's process table:

- **P0**: arrival=0, burst=5, priority=1
- **P1**: arrival=1, burst=3, priority=2

`sched_create_process()` assigns each process a unique PID, initializes its PCB with all timing fields zeroed, and marks it `STATE_NEW`.

### Step 2 — CPU Scheduling (Subsystem A)

`sched_run()` executes FCFS on the process table. P0 arrives first and runs 0–5; P1 arrives at t=1 but waits until P0 finishes, then runs 5–8. The scheduler computes per-process waiting and turnaround times and records the Gantt timeline. The results are printed immediately after the simulation.

### Step 3 — Memory Access (Subsystem B)

P0 accesses logical address `0x0100` (page 1, offset 0). Because no pages are loaded at startup, the first access is a **page fault**: page 1 is loaded into frame 0 and the physical address `0x0000` is returned. The second access to the same address is a **hit**, demonstrating the page table lookup without a fault. Hit/fault statistics are printed after both accesses.

### Step 4 — Synchronization & Protection (Subsystem C)

Before P0 is permitted to write to a shared resource, a permission check is applied via `sync_check_permission()`. The full role–action matrix is printed to show which roles are authorized. An EDITOR then acquires the write lock (`sync_begin_write` / `sync_end_write`) successfully. A USER role attempts the same write and is denied, demonstrating the protection layer.

---

## Integration Points

| Checkpoint                 | API Call                               | Result                     |
| -------------------------- | -------------------------------------- | -------------------------- |
| Process created            | `sched_create_process(0, 5, 1)`        | PID 0 assigned             |
| Process scheduled          | `sched_run()`                          | FCFS Gantt produced        |
| Memory accessed            | `mem_access(0x0100)`                   | Page fault → hit on repeat |
| Permission checked         | `sync_check_permission(EDITOR, WRITE)` | ALLOWED                    |
| Write lock acquired        | `sync_begin_write / sync_end_write`    | OK                         |
| Unauthorized write blocked | `sync_begin_write(USER, WRITE)`        | DENIED                     |

---

## Known Limitations

**No shared process identity across subsystems.** The scheduler, memory manager, and sync module each maintain independent state. The memory access in Step 3 is not issued by P0 programmatically — it is called by the demo script to represent what P0 would do. A full integration would require the scheduler to invoke `mem_access` on behalf of a running process.

**No per-process address space.** The memory subsystem has a single global page table and frame pool. In a real OS each process would have its own page table. As implemented, all processes share the same physical frames with no isolation between them.

**Synchronization is single-threaded.** The sync module uses POSIX semaphores and a mutex correctly, but the vertical slice exercises them from a single thread. True concurrent behavior (multiple readers and writers contending simultaneously) is not demonstrated here.

---

## Planned Fixes / Future Work

- Pass PIDs through to `mem_access` so the memory subsystem can log which process caused each page fault.
- Add a per-process page table in the memory subsystem to enforce address space isolation between processes.
- Extend the vertical slice demo with a multi-threaded scenario (two goroutines representing P0 and P1 contending on the sync module) to validate the readers-writers implementation under real concurrency.
