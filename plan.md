# propd Design Review & Improvement Plan

> 2026-05-24 | 基于 v0.1 架构分析

---

## 1. 当前协议总览

### IO 协议 (SOCK_STREAM)

```
io_package_t (固定 269 字节):
┌──────┬──────────┬──────────────────┬─────────────────┐
│ type │ created  │  key[NAME_MAX]   │ value (仅header) │
│  1B  │   8B     │      255B        │      5B         │
└──────┴──────────┴──────────────────┴─────────────────┘
  + 可选: value.length 字节数据 (仅 set)
```

Server 响应:
- get: `duration(8B)` → `value_t+data` → `result(4B)`  (3 段)
- set/del: `result(4B)`  (1 段)

### Ctrl 协议 (SOCK_DGRAM)

```
ctrl_package_t (最小 256 字节):
┌──────┬──────────────────────────────────┐
│ type │  name[255] 或 register_child_t   │
│  1B  │            255B+                 │
└──────┴──────────────────────────────────┘
```

- 简单操作 = 256B, register_child = 264B + N×255B
- Server 响应: `result(4B)` via sendto

---

## 2. 协议改进

### 2.1 Version 字段 (P0)

当前无版本标识，协议演进困难。

**建议**: 两种包首字节插入 `uint8_t version`。当前代码标记 version=0，新实现走 version=1+。

### 2.2 io_package.value 命名 (P0)

`value_t value` 只含 header，实际数据在单独 send 中。

**建议**: 重命名为 `value_head`。

### 2.3 Ctrl 显式长度字段 (P1)

两端独立计算包长，无运行时校验，可能截断。

**建议**: 头部加 `uint16_t payload_length`。

### 2.4 Server 响应统一 (P1)

get 响应 3 段 (duration→value→result)，set/del 仅 1 段 (result)，不对称。

**建议**: 统一为 `struct io_response { result; duration; value_length; value[]; }`。

### 2.5 IO 变长 key (P2)

固定 key[255] 浪费带宽。

**建议**: 改为 TLV：`type + created + key_length + value_length + key[] + data[]`。

### 2.6 us_read_auto 隐式依赖 (P2)

假设头部最后 4 字节 = 数据长度，依赖 value_t 布局。

**建议**: 加 `static_assert` 或显式 `data_length` 字段。

### 2.7 Ctrl 加时间戳 (P3)

与 io_package 保持一致。

### 2.8 大端兼容 (P2)

**建议**: 加 `htonl`/`ntohl`（或声明 LE-only）。

---

## 3. 建议的目标协议 (v2)

```c
// IO 请求
struct io_request {
    uint8_t     version;       // =2
    io_type_t   type;
    timestamp_t created;
    uint16_t    key_length;
    uint32_t    value_length;
    char        key[];
    // 后跟 value_length 字节数据
};

// IO 响应
struct io_response {
    int32_t     result;
    timestamp_t duration;
    uint32_t    value_length;
    uint8_t     value[];
};

// Ctrl 请求
struct ctrl_request {
    uint8_t     version;
    ctrl_type_t type;
    timestamp_t created;
    uint16_t    payload_length;
    union { struct register_child { ... }; char name[]; };
};

// Ctrl 响应
struct ctrl_response {
    int32_t     result;
    uint32_t    data_length;
    uint8_t     data[];
};
```

---

## 4. 注册/反注册机制

### 4.1 组包/解包封装

当前每个 ctrl API 重复 `ctrl_init→填字段→ctrl_update→ctrl_final0`。

**建议**: 提取 `ctrl_pack_register_child()` 等辅助函数。

### 4.2 解包边界校验

`register_child()` 直接用 `child->num_prefix` 遍历，未校验是否在 recv 范围内。

**建议**: 校验 `(num_cache_now + num_prefix) * NAME_MAX <= actual_payload_length`。

### 4.3 io_update 同步阻塞

Ctrl worker 中 `io_update()` 连接子节点并等待响应，可能阻塞线程池。

**建议**: 改为异步（提交回线程池），先返回 "accepted"。

### 4.4 反注册 zombie 机制

`route_list_unregister` 遇 nref>0 直接返回 EBUSY。

**建议**: 加 `bool zombie` 标记——unregister 发现 nref>0 设 zombie=true；`route_deref` 发现 nref==0 && zombie 自动删除。

---

## 5. Ctrl 身份追溯

### 决策：DGRAM 保持，用 SCM_CREDENTIALS

`SO_PASSCRED` + `recvmsg` 获取内核级验证的 pid/uid/gid：

```c
setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &(int){1}, sizeof(int));
struct msghdr msg = {0};
char cbuf[CMSG_SPACE(sizeof(struct ucred))];
// ... recvmsg 后提取 SCM_CREDENTIALS
logfI("[ctrl] recv from p%d,u%d,g%d", cred->pid, cred->uid, cred->gid);
```

### STREAM/DGRAM 身份追溯统一

底层机制不同（STREAM 绑连接, DGRAM 绑消息），但可抽取统一接口：

```c
// peer_cred.h

/** STREAM: accept 后获取已连接对端身份 */
int peer_cred_get(int fd, struct ucred *cred);

/** DGRAM: 接收数据报同时获取发送方身份 */
ssize_t peer_cred_recv(int fd, void *buf, size_t len, int flags,
                        struct sockaddr *src, socklen_t *addrlen,
                        struct ucred *cred);
```

统一日志格式：

```c
#define logFmtCred "p%d,u%d,g%d"
#define logArgCred(c) (c).pid, (c).uid, (c).gid
```

### 定位：审计日志，不做强制访问控制

`cred_check` 保持 stub。Unix socket 文件权限即为访问控制。

---

## 6. 注册 Append 模式

### 新增 `_ctrl_register_child_append` (=6)

| 模式 | 注册名已存在 | 注册名不存在 |
|------|------------|------------|
| 普通 (type=0) | 清理旧的，创建新的 | 创建新的 |
| 追加 (type=6) | 追加 prefix/cache_now | 返回 ENOENT |

包结构复用 `ctrl_package_register_child_t`。

```
# 启动
prop ctrl register_child db-server /db/* /config/*
# 热加载
prop ctrl register_child_append db-server /metrics/*
```

协议层不需要分片；一次 append 装不下由调用方多次 append。

---

## 7. 其他架构议题

### 7.1 路由表

当前 LIST O(n)。建议用 Trie/Radix Tree 或至少排序+二分。

### 7.2 named_mutex

✅ 实现质量高 — RB-tree + per-key mutex + nref。无需改进。

### 7.3 缓存主动失效

TTL-only 不够。新增 `_ctrl_cache_invalidate` 消息，子节点 set 后通知父节点失效缓存。

### 7.4 统一错误码

混用 errno/EIO/ENOENT/-1。建议定义 `PROPD_ERR_*` 枚举（负值空间）。

### 7.5 测试

后期建 `tests/unit/` + `tests/integration/`。

---

## 8. 已完成

- [x] 目录结构重组 (`include/` `src/` `vendor/`)
- [x] `libprop` / `libpropd` 分离
- [x] `builtin/unix.c` 复用 `unix_stream.h`

---

## 9. 待办优先级

| 优先级 | 项目 |
|--------|------|
| P0 | 加 version 字段 |
| P0 | io_package.value → value_head |
| P0 | Ctrl 解包边界校验 |
| P1 | Ctrl payload_length |
| P1 | io_response 统一 |
| P1 | register_child_append |
| P1 | Ctrl SCM_CREDENTIALS 身份追溯 |
| P1 | STREAM/DGRAM peer_cred 统一接口 |
| P1 | 反注册 zombie 机制 |
| P2 | IO 变长 key |
| P2 | Socket 超时 |
| P2 | 子节点健康检查 |
| P3 | 缓存主动失效 |
| P3 | 统一错误码 |
| P3 | 路由表 Trie |
| P4 | 测试 |
