# Minimal Logger

> MIT License Copyright (c) 2025 kioz.wang

# Description

最通用、最迷你的日志框架。可用于 `C`, `C++`。

## Test and Run

```shell
$ rm -f *.gch *.o a.out && gcc -D__TEST_LOGGER__ -DMLOGGER_ENV=\"log2stderr\" -DMLOGGER_COLOR -DMLOGGER_TIMESTAMP -DMLOGGER_LEVEL *.[ch]
$ ./a.out
message level ERRO
message level WARN
message level INFO

[2e71497f8][E]ERRO
[2e71497ff][W]WARN
[2e7149801][I]INFO
$ log2stderr=DEBUG ./a.out
message level ERRO
message level ERRO
message level WARN
message level WARN
message level INFO
message level INFO
message level VERB
message level DEBG


[2e78c987a][E]ERRO
[2e78c987a][E]ERRO
[2e78c9880][W]WARN
[2e78c9880][W]WARN
[2e78c9882][I]INFO
[2e78c9882][I]INFO
[2e78c9885][V]VERB
[2e78c9886][D]DEBG
```

## Build as library

If you want, just do it.

## Build with your source codes

This is a best practice.

# API

## `mlog_level_t`, `mlogger_f`

```c
enum mlog_level {
  MLOG_ERRO,
  MLOG_WARN,
  MLOG_INFO,
  MLOG_VERB,
  MLOG_DEBG,
};
typedef uint8_t mlog_level_t;

typedef void (*mlogger_f)(const char *msg);
```

仅使用两个全局变量：
- 日志等级：`g_log_level`，默认为 `MLOG_DEBUG`
- 输出函数：`g_logger`，默认为 `fputs(msg, stdout)`

## `mlog_level_parse`

```c
mlog_level_t mlog_level_parse(const char *s);
```

解析字符串到 `mlog_level_t`，一般用在命令行解析函数中。

## `MLOGGER_FUNC(logf)`

```c
void MLOGGER_FUNC(logf)(mlog_level_t lvl, const char *fmt, ...);
```

当 `lvl` 大于等于 `g_log_level` 时，调用 `g_logger` 输出格式化消息。

宏函数 `logfE/W/I/V/D` 是省略了 `lvl` 的写法，并追加了一个换行。

## `MLOGGER_FUNC(set_logger)`

```c
void MLOGGER_FUNC(set_logger)(mlog_level_t lvl, mlogger_f f);
```

修改全局变量。`lvl` 具有防呆设计；`f` 传入 `NULL` 时，不更新 `g_logger`。

## `MLOGGER_FUNC(set_out_logger)`

```c
void MLOGGER_FUNC(set_out_logger)(void (*f)(mlog_level_t, mlogger_f));
```

将日志等级和输出函数传递到另一个模块。

# Configuration

## `MLOGGER_PREFIX`

为避免移植时出现符号冲突（像`logf`这种名字太常见了），请在头文件中修改宏定义。

> 默认值为 `_`

## `MLOGGER_FMT_MAX`

在函数栈内存中完成消息格式化，该宏指定了占用栈内存的大小。

> 默认值为 `1024`

## `MLOGGER_ENV`

这个宏指定了一个环境变量，`MLOGGER_FUNC(logf)` 将读取这个环境变量，并翻译为 `mlog_level_t stderr_level`。

在 `MLOGGER_FUNC(logf)` 中，当 `lvl` 大于等于 `stderr_level` 时，调用 `fputs(line, stderr)` 输出格式化消息。

## 仅对 `logfE/W/I/V/D` 有效的

> 以下配置可根据需要修改头文件

- `MLOGGER_COLOR` 使能颜色
- `MLOGGER_TIMESTAMP` 开头显示时间戳：十六进制 monotonic 时间，单位：ms
- `MLOGGER_LEVEL` 开头显示日志等级：`[E/W/I/V/D]`

### 翻译规则

单词

`"ERRO", "WARN", "INFO", "VERB", "DEBG"` 依次对应 `mlog_level_t` 枚举值。不匹配的单词将按照数字翻译。

数字

- 自动推断进制
- 按数值大小一一映射到 `mlog_level_t` 枚举值。
- 非法数字将映射到 `MLOG_DEBG`
- 过大的数字将映射到 `MLOG_DEBG`
