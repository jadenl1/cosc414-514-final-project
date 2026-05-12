# MOSS – Mini Operating System Services Simulator

A user-space OS simulator built for COSC 414/514 – Operating Systems.
MOSS lets you experiment with core OS policies — CPU scheduling, memory management,
and synchronization — and observe their effects on performance and resource usage.

---

## Team

| Member | Subsystem |
|---|---|
| Jaden Leonard | Subsystem A – Process Management & CPU Scheduling |
| Amir Taylor | Subsystem B – Memory Management & Virtual Memory |
| Fletcher Baccus | Subsystem B – Memory Management & Virtual Memory |
| Uzoma Bruce | Subsystem C – Synchronization & Protection |

---

## Build

Requires `g++` (version 11 or later) and `make`. Target platform: **Ubuntu 22.04 LTS**.

```bash
make            # build unified moss binary (Part II placeholder)
make demos      # build all three Part I demo binaries
make test       # build and run full test suite
make clean      # remove all build artifacts
```

---

## Run

**Part I — individual subsystem demos:**

```bash
./demo_sched   # Subsystem A – Process Management & CPU Scheduling
./demo_mem     # Subsystem B – Memory Management & Virtual Memory
./demo_sync    # Subsystem C – Synchronization & Protection
```

**Part II unified CLI** (placeholder — full integration coming in Part II):
```bash
./moss
```

---

## Subsystem A – Scheduling (`./demo_sched`)

### Commands
```
create_process <arrival> <burst> <priority>   create a new process
schedule <FCFS|RR|PRIORITY> [quantum]         set scheduling algorithm
run                                           run the simulation
gantt                                         print Gantt chart
stats                                         print per-process statistics
reset                                         clear processes, keep algorithm
help                                          show all commands
exit                                          quit
```

### Example
```
moss> schedule RR 2
  algorithm set to RR (quantum=2)
moss> create_process 0 5 0
  created process P0
moss> create_process 0 3 0
  created process P1
moss> create_process 0 8 0
  created process P2
moss> run
  simulation complete

  | P0   | P1   | P2   | P0   | P1   | P2   | P0   | P2   | P2   |
  0     2     4     6     8     9     11    12    14    16

  PID   Wait        Turnaround
  ----------------------------------
  0     7           12
  1     6           9
  2     8           16
  ----------------------------------
  Avg wait: 7.00   Avg turnaround: 12.33
```

---

## Subsystem B – Memory (`./demo_mem`)

### Commands
```
algorithm <FIFO|LRU>    set page replacement algorithm
access <address>        access a logical address (0–65535)
frames                  show current frame contents
stats                   show hit/fault statistics
reset                   clear frames and stats, keep algorithm
demo                    run a preset page reference sequence
help
exit
```

### Example
```
mem> algorithm LRU
  algorithm set to LRU
mem> access 256
  FAULT  page=1  offset=0  phys=0x0
  Frames: [  1  --  --  -- ]
mem> access 256
  HIT    page=1  offset=0  phys=0x0
  Frames: [  1  --  --  -- ]
mem> stats
  Hits: 1  Faults: 1  Hit rate: 50.0%
```

---

## Subsystem C – Sync (`./demo_sync`)

### Commands
```
init                                    initialize sync state
read   <USER|EDITOR|ADMIN>             attempt a read  (begin + end)
write  <USER|EDITOR|ADMIN>             attempt a write (begin + end)
check  <USER|EDITOR|ADMIN> <READ|WRITE|MANAGE>  permission check only
permissions                            show full permission matrix
status                                 show active readers/writers
demo                                   run preset scenarios
reset                                  destroy and reinitialize
help
exit
```

### Example
```
sync> init
  sync state initialized
sync> check USER WRITE
  USER WRITE: DENIED
sync> write EDITOR
  begin_write (EDITOR): OK
  end_write: OK
sync> write USER
  begin_write (USER): DENIED (rc=-1)
sync> permissions
  Role      READ    WRITE   MANAGE
  ------------------------------------
  USER      YES     no      no
  EDITOR    YES     YES     no
  ADMIN     YES     YES     YES
sync> demo
  [runs 4 preset scenarios]
```

> **Note:** Subsystem C uses POSIX semaphores (`sem_init`) which require
> **Ubuntu 22.04**. Not supported on macOS.

---

## Tests

```bash
make test
```

Runs all 113 tests across all three subsystems. Sync tests auto-skip on macOS
(requires Ubuntu for `sem_init` support).

```
════ Subsystem A – Scheduling ════   73 tests
════ Subsystem B – Memory ════       33 tests
════ Subsystem C – Sync ════          7 tests (skipped on macOS)
```

---

## Project Structure

```
project-root/
├── include/
│   ├── sched.h             # Subsystem A public API
│   ├── mem.h               # Subsystem B public API
│   └── sync.h              # Subsystem C public API
├── src/
│   ├── main.cpp            # Part II unified CLI (placeholder)
│   ├── demo_sched.cpp      # Part I demo – Subsystem A
│   ├── demo_mem.cpp        # Part I demo – Subsystem B
│   ├── demo_sync.cpp       # Part I demo – Subsystem C
│   ├── sched/
│   │   └── sched.cpp       # Process management & CPU scheduling
│   ├── mem/
│   │   └── mem.cpp         # Memory management & virtual memory
│   └── sync/
│       └── sync.cpp        # Synchronization & protection
├── tests/
│   └── basic_tests.cpp     # 113 tests across all three subsystems
├── docs/
│   ├── api.md              # Full system API reference
│   ├── design-docs/
│   │   ├── SubsystemA-Design.md      # Subsystem A design document
│   │   ├── SubsystemB_DesignDoc.pdf  # Subsystem B design document
│   │   └── SubsystemC_DesignDoc.docx # Subsystem C design document
│   ├── reflections/
│   │   ├── Reflection-SubsystemA-JadenLeonard.docx.pdf
│   │   ├── Amir Reflection SS B.pdf
│   │   └── Reflection of Sub SYstem B.pdf
│   └── cross-reviews/
│       ├── Jaden Leonard Cross-Review of Subsystem C.pdf
│       └── Subsystem A Cross Review.pdf
├── Makefile
└── README.md
```

---

## Documentation

| Document | Description |
|---|---|
| [`docs/design-docs/SubsystemA-Design.md`](docs/design-docs/SubsystemA-Design.md) | Subsystem A design, algorithms, and API reference |
| `docs/design-docs/SubsystemB_DesignDoc.pdf` | Subsystem B design document |
| `docs/design-docs/SubsystemC_DesignDoc.docx` | Subsystem C design document |
| [`docs/api.md`](docs/api.md) | Full system API reference |
| [`docs/vertical_slice.md`](docs/vertical_slice.md) | Vertical slice walkthrough, integration points, and known limitations |
| `docs/reflections/` | Individual reflections (one per team member) |
| `docs/cross-reviews/` | Peer cross-reviews |
| Project spec PDF | Full project requirements |
