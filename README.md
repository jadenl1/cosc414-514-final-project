# MOSS – Mini Operating System Services Simulator

A user-space OS simulator built for COSC 414/514 – Operating Systems.
MOSS lets you experiment with core OS policies — CPU scheduling, memory management,
and synchronization — and observe their effects on performance and resource usage.

---

## Team

| Member | Subsystem |
|---|---|
| Jaden Leonard | Subsystem A – Process Management & CPU Scheduling |
| | Subsystem B – Memory Management & Virtual Memory |
| | Subsystem C – Synchronization & Protection |

---

## Build

Requires `g++` (version 11 or later) and `make`.

```bash
make
```

Produces the `moss` binary in the project root. To clean build artifacts:

```bash
make clean
```

---

## Run

**Part II unified CLI** (not yet implemented):
```bash
./moss
```

**Part I individual subsystem demos:**
```bash
./demo_sched   # Subsystem A – Process Management & CPU Scheduling
./demo_mem     # Subsystem B – Memory Management & Virtual Memory
./demo_sync    # Subsystem C – Synchronization & Protection
```

Build all demos at once:
```bash
make demos
```

---

## demo_sched Commands

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

### Example Session

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

## Run Tests

```bash
g++ -Wall -Wextra -I./include -std=c++17 -o tests/run_tests tests/basic_tests.cpp src/sched/sched.cpp && ./tests/run_tests
```

---

## Project Structure

```
project-root/
├── include/
│   ├── sched.h       # Subsystem A public API
│   ├── mem.h         # Subsystem B public API
│   └── sync.h        # Subsystem C public API
├── src/
│   ├── main.cpp        # Part II unified CLI (placeholder)
│   ├── demo_sched.cpp  # Part I demo – Subsystem A
│   ├── demo_mem.cpp    # Part I demo – Subsystem B
│   ├── demo_sync.cpp   # Part I demo – Subsystem C
│   ├── sched/
│   │   └── sched.cpp   # Process management & CPU scheduling
│   ├── mem/
│   │   └── mem.cpp     # Memory management & virtual memory
│   └── sync/
│       └── sync.cpp    # Synchronization & protection
├── tests/
│   └── basic_tests.cpp
├── docs/
│   ├── design.md     # Subsystem A design document & API reference
│   ├── api.md        # Full system API reference (Part II)
│   └── reflection.md # Individual reflection
├── Makefile
└── README.md
```

---

## Documentation

- **Subsystem A design & API reference:** [`docs/design.md`](docs/design.md)
- **Project specification:** `Cosc 414_514 Semester Project – Mini Operating System Services Simulator.pdf`
