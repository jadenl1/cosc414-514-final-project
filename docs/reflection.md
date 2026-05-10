Jaden Leonard
Dr. Sarker
COSC 514 – Operating Systems
10 May 2026

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Individual Reflection: Subsystem A – Process Management & CPU Scheduling

&nbsp;&nbsp;&nbsp;&nbsp;Designing and implementing Subsystem A of the Mini Operating System Services
Simulator (MOSS) required careful consideration of data structure design, algorithm
selection, and API stability. This reflection describes the key design decisions made
during development, the assumptions that shaped the implementation, and the limitations
that would be addressed given more time.

&nbsp;&nbsp;&nbsp;&nbsp;The most foundational decision was how to represent a process. The Process Control
Block (PCB) struct was designed to be self-contained, storing both the static attributes
of a process — arrival time, burst time, and priority — and its runtime state, including
remaining time, start time, finish time, waiting time, and turnaround time. Keeping all
of this in a single struct simplified the implementation of each scheduling algorithm,
since every piece of information needed for scheduling decisions and stat computation
was accessible in one place. The alternative, splitting static and dynamic data into
separate structures, would have added indirection without meaningful benefit at this
scale.

&nbsp;&nbsp;&nbsp;&nbsp;The decision to implement Priority scheduling as non-preemptive was deliberate.
A preemptive version would require interrupting a running process mid-burst when a
higher-priority process arrives, which in a real OS would involve saving CPU register
state. In a logical simulator with no real execution context, this would only add
complexity to the simulation loop without modeling anything meaningful that a
non-preemptive version does not already demonstrate. Non-preemptive Priority scheduling
still correctly illustrates how priority values influence dispatch order and the
resulting impact on waiting and turnaround times.

&nbsp;&nbsp;&nbsp;&nbsp;One unexpected challenge arose from a naming conflict between the `SCHED_RR`
enum value and the POSIX macro of the same name defined in `<pthread.h>`. This was
discovered during the first compile on macOS and resolved by prefixing all algorithm
enum values with `ALGO_` rather than `SCHED_`. While this particular conflict would
not surface on all platforms, it highlighted the importance of choosing names that do
not collide with system-level identifiers — a real concern when writing code intended
to be portable across environments.

&nbsp;&nbsp;&nbsp;&nbsp;The API was designed around a key constraint from the project specification: core
functions must not print to stdout or stderr. All output is the responsibility of the
caller. This led to the `GanttEntry` struct and the two-mode design of `sched_get_gantt()`,
which allows the caller to first query how many entries exist and then allocate an
appropriately sized buffer. This pattern avoids fixed-size assumptions and keeps the
subsystem decoupled from any particular output format, which will matter during
integration in Part II when a unified CLI will handle all printing.

&nbsp;&nbsp;&nbsp;&nbsp;The primary limitation of the current implementation is that time is modeled as
a discrete integer, which means burst times and arrival times must be whole numbers.
Real scheduling systems operate in nanosecond-resolution and must account for context
switch overhead, I/O bursts, and dynamic priority aging — none of which are modeled
here. Additionally, the Priority scheduling algorithm does not implement aging, meaning
a low-priority process can be indefinitely starved if higher-priority processes
continuously arrive. Given more time, adding aging to the Priority scheduler and
implementing a Multilevel Feedback Queue (MLFQ) would be the next steps, as MLFQ
addresses starvation while combining the strengths of both priority and round-robin
scheduling.

&nbsp;&nbsp;&nbsp;&nbsp;Overall, this subsystem successfully demonstrates the core mechanics of CPU
scheduling in a clean, testable, and integration-ready form. The 73-test suite covering
all functions, edge cases, and error paths provides confidence that the API behaves
correctly under both normal and boundary conditions, which will be essential when
Subsystem A is integrated with the memory management and synchronization subsystems
in Part II.
