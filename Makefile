CXX      = g++
CXXFLAGS = -Wall -Wextra -I./include -std=c++17
LDFLAGS  = -lpthread

# ── Unified binary (Part II) ──────────────────────────────────────────────────

SUBSYS_SRCS = src/sched/sched.cpp src/mem/mem.cpp src/sync/sync.cpp
SUBSYS_OBJS = $(SUBSYS_SRCS:.cpp=.o)
MOSS_OBJS   = src/main.o $(SUBSYS_OBJS)

# ── Demo binaries (Part I) ────────────────────────────────────────────────────

DEMOS = demo_sched demo_mem demo_sync

.PHONY: all demos clean

all: moss

demos: $(DEMOS)

moss: $(MOSS_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

demo_sched: src/demo_sched.o src/sched/sched.o
	$(CXX) $(CXXFLAGS) -o $@ $^

demo_mem: src/demo_mem.o src/mem/mem.o
	$(CXX) $(CXXFLAGS) -o $@ $^

demo_sync: src/demo_sync.o src/sync/sync.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(MOSS_OBJS) src/demo_sched.o src/demo_mem.o src/demo_sync.o \
	      moss $(DEMOS)
