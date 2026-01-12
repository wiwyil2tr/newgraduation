# 后端 Makefile

CC = gcc
CFLAGS = -Wall -fPIC -I./include -g -D_GNU_SOURCE
LDFLAGS = -ldl -lpthread -ljson-c
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .

# 源文件
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/list.c \
       $(SRC_DIR)/framework/plugin_manager.c \
       $(SRC_DIR)/framework/plugin_loader.c \
       $(SRC_DIR)/framework/utils.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/pentk

# 默认目标
all: $(TARGET)

# 链接可执行文件
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

# 编译源文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# 安装
install: all
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
