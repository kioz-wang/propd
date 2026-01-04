# lib/Makefile
.PHONY: all directories build-only clean

# 编译器设置
CC ?= gcc
CFLAGS_DEBUG ?= -g -O0
CFLAGS_RELEASE ?= -DNDEBUG
CFLAGS ?= -std=gnu11 -Wall -Wextra -fPIC -DMLOGGER_COLOR -DMLOGGER_TIMESTAMP -DMLOGGER_LEVEL $(CFLAGS_RELEASE)

# 目录
LIB_SRC_DIR = .
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj/lib
LIB_DIR = $(BUILD_DIR)/lib

# 递归查找所有 .c 和 .h 文件
SRCS = $(shell find $(LIB_SRC_DIR) -name '*.c' 2>/dev/null | sort)
HEADERS = $(shell find $(LIB_SRC_DIR) -name '*.h' 2>/dev/null | sort)

# 生成目标文件列表（保持目录结构）
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

# 目标
TARGET = $(LIB_DIR)/libpropd.so

# 库编译选项
# ASANFLAGS = -fsanitize=address -fsanitize-address-use-after-scope
LDFLAGS = -shared -pthread $(ASANFLAGS)
LIBS = -lcjson

# 包含路径
INCLUDES = -I.

# 默认目标：构建
all: build-only

# 只构建，不安装
build-only: directories $(TARGET)

# 创建构建目录（不创建安装目录）
directories:
	@mkdir -p $(LIB_DIR)
	@for src in $(SRCS); do \
		dir=$$(dirname $(OBJ_DIR)/$$src); \
		mkdir -p $$dir 2>/dev/null || true; \
	done

# 编译规则（支持子目录）
$(OBJ_DIR)/%.o: %.c
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 链接库
$(TARGET): $(OBJS)
	@echo "  LD  $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "✓ Library built: $@"

# 清理
clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(TARGET)
	@echo "✓ Library cleaned"

# 导出变量
export TARGET
export LIB_DIR
export BUILD_DIR
export INCLUDES
