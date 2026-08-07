# src/Makefile — builds libprop (thin client) and libpropd (full server)
.PHONY: all directories build-only clean

# 编译器设置
CC ?= gcc
CFLAGS_DEBUG ?= -g -O0
CFLAGS_RELEASE ?= -DNDEBUG
CFLAGS ?= -std=gnu11 -Wall -Wextra -fPIC -DMLOGGER_COLOR -DMLOGGER_TIMESTAMP -DMLOGGER_LEVEL $(CFLAGS_DEBUG)

# 目录
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj/src
LIB_DIR = $(BUILD_DIR)/lib
VENDOR_DIR = ../vendor

# ── libprop (thin client) ──────────────────────────────────────────────
PROP_SRCS = $(shell find client -name '*.c' 2>/dev/null | sort)
PROP_SRCS += $(shell find $(VENDOR_DIR)/mlogger -name '*.c' 2>/dev/null | sort)
PROP_SRCS += $(shell find $(VENDOR_DIR)/memio -name '*.c' 2>/dev/null | sort)
PROP_OBJS = $(PROP_SRCS:%.c=$(OBJ_DIR)/%.o)
PROP_TARGET = $(LIB_DIR)/libprop.so

# ── libpropd (full server) ─────────────────────────────────────────────
PROPD_SRCS = $(shell find server -name '*.c' 2>/dev/null | sort)
PROPD_OBJS = $(PROPD_SRCS:%.c=$(OBJ_DIR)/%.o)
PROPD_TARGET = $(LIB_DIR)/libpropd.so

# 库编译选项
LDFLAGS = -shared -pthread $(ASANFLAGS)
LIBS = -lcjson

# 包含路径
INCLUDES = -I. -Iclient -Iserver -Iserver/infra -I../include/propd -I$(VENDOR_DIR)/mlogger -I$(VENDOR_DIR)

# 默认目标：构建
all: build-only

# 只构建，不安装
build-only: directories $(PROP_TARGET) $(PROPD_TARGET)

# 创建构建目录
directories:
	@mkdir -p $(LIB_DIR)
	@for src in $(PROP_SRCS) $(PROPD_SRCS); do \
		dir=$$(dirname $(OBJ_DIR)/$$src); \
		mkdir -p $$dir 2>/dev/null || true; \
	done

# 编译规则（支持子目录）
$(OBJ_DIR)/%.o: %.c
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 链接 libprop
$(PROP_TARGET): $(PROP_OBJS)
	@echo "  LD  $@"
	@$(CC) $(LDFLAGS) -o $@ $(PROP_OBJS) $(LIBS)
	@echo "✓ libprop built: $@"

# 链接 libpropd
$(PROPD_TARGET): $(PROPD_OBJS) $(PROP_TARGET)
	@echo "  LD  $@"
	@$(CC) $(LDFLAGS) -o $@ $(PROPD_OBJS) -L$(LIB_DIR) -lprop $(LIBS)
	@echo "✓ libpropd built: $@"

# 清理
clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(PROP_TARGET) $(PROPD_TARGET)
	@echo "✓ Library cleaned"

# 导出变量
export PROP_TARGET PROPD_TARGET LIB_DIR BUILD_DIR INCLUDES
