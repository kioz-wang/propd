# bin/Makefile
.PHONY: all build-only clean check-lib

# 编译器设置
CC ?= gcc
CFLAGS ?= -std=gnu11 -Wall -Wextra \
	-g -O0

# 目录
BIN_SRC_DIR = .
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj/bin
BIN_DIR = $(BUILD_DIR)/bin

# 包含路径
INCLUDES = -I../lib

# 从 lib 导入
LIB_TARGET ?= $(BUILD_DIR)/lib/libpropd.so
LIB_DIR ?= $(BUILD_DIR)/lib

# 链接选项
# SANFLAGS = -fsanitize=address -fsanitize-address-use-after-scope
LDFLAGS = -L$(LIB_DIR) -lpropd -Wl,-rpath -Wl,$(realpath $(LIB_DIR)) $(SANFLAGS)

# 递归查找所有源文件
PROP_SRCS = $(shell find prop -name '*.c' 2>/dev/null | sort)
PROPD_SRCS = $(shell find propd -name '*.c' 2>/dev/null | sort)

# 生成目标文件列表
PROP_OBJS = $(PROP_SRCS:%.c=$(OBJ_DIR)/%.o)
PROPD_OBJS = $(PROPD_SRCS:%.c=$(OBJ_DIR)/%.o)

# 目标文件
PROP_TARGET = $(BIN_DIR)/prop
PROPD_TARGET = $(BIN_DIR)/propd

# 默认目标：构建
all: build-only

# 检查库是否存在
check-lib:
	@if [ ! -f "$(LIB_TARGET)" ]; then \
		echo "Error: Library $(LIB_TARGET) not found!"; \
		echo "Run 'make lib' first."; \
		exit 1; \
	fi

# 只构建，不安装
build-only: check-lib directories $(PROP_TARGET) $(PROPD_TARGET)

# 创建构建目录（不创建安装目录）
directories:
	@mkdir -p $(BIN_DIR)/
	@for src in $(PROP_SRCS) $(PROPD_SRCS); do \
		dir=$$(dirname $(OBJ_DIR)/$$src); \
		mkdir -p $$dir 2>/dev/null || true; \
	done

# 编译 prop 源文件
$(OBJ_DIR)/prop/%.o: prop/%.c
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 编译 propd 源文件
$(OBJ_DIR)/propd/%.o: propd/%.c
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 构建 prop 可执行文件
$(PROP_TARGET): $(PROP_OBJS)
	@echo "  LD  $@"
	@$(CC) -o $@ $(PROP_OBJS) $(LDFLAGS)
	@echo "✓ Built: $@"

# 构建 propd 可执行文件
$(PROPD_TARGET): $(PROPD_OBJS)
	@echo "  LD  $@"
	@$(CC) -o $@ $(PROPD_OBJS) $(LDFLAGS)
	@echo "✓ Built: $@"

# 清理
clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(PROP_TARGET)
	@rm -f $(PROPD_TARGET)
	@echo "✓ Binaries cleaned"
