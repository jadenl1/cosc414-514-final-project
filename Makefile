CC      = gcc
CFLAGS  = -Wall -Wextra -I./include
TARGET  = moss

SRCS    = $(shell find src -name '*.c')
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
