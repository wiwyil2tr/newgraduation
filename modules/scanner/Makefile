# 端口扫描器模块 Makefile

CC = gcc
CFLAGS = -Wall -fPIC -I../../backend/include -pthread -D_GNU_SOURCE
LDFLAGS = -shared -lpthread
TARGET = port_scanner.so
SRCS = port_scanner.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install:
	cp $(TARGET) ../../../modules/

.PHONY: all clean install
