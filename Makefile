CXX     = g++
CXXFLAGS = -Wall -Wextra -I./include -std=c++17
TARGET  = moss

SRCS    = $(shell find src -name '*.cpp')
OBJS    = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
