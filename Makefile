# 顶层 Makefile
.PHONY: all lib app clean help

# 编译器设置
CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -fPIC

# 项目目录
BUILD_DIR = build

# 默认目标
all: lib app

# 构建库
lib:
	@echo "=== Building library ==="
	@$(MAKE) --no-print-directory -C lib

# 构建可执行文件
app:
	@echo "=== Building binaries ==="
	@$(MAKE) --no-print-directory -C app

# 清理
clean:
	@echo "=== Cleaning ==="
	@$(MAKE) --no-print-directory -C lib clean
	@$(MAKE) --no-print-directory -C app clean
	@rm -rf $(BUILD_DIR)

# 帮助
help:
	@echo "Targets:"
	@echo "  all           - Build everything"
	@echo "  lib           - Build only library"
	@echo "  app           - Build only binaries"
	@echo "  clean         - Clean everything"
	@echo "  help          - Show this help"
