CXX      = g++
CXXFLAGS = -Wall -Wextra -iquote ./include -std=c++17
LDFLAGS  = -lpthread

# ── Unified binary (Part II) ──────────────────────────────────────────────────

SUBSYS_SRCS = src/sched/sched.cpp src/mem/mem.cpp src/sync/sync.cpp
SUBSYS_OBJS = $(SUBSYS_SRCS:.cpp=.o)
MOSS_OBJS   = src/main.o $(SUBSYS_OBJS)

# ── Demo binaries (Part I) ────────────────────────────────────────────────────

DEMOS = demo_sched demo_mem demo_sync

.PHONY: all demos test clean

all: moss

demos: $(DEMOS)

moss: $(MOSS_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test: $(SUBSYS_OBJS)
	$(CXX) $(CXXFLAGS) -o tests/run_tests tests/basic_tests.cpp $(SUBSYS_OBJS) $(LDFLAGS)
	./tests/run_tests

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
