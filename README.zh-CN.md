# PingServer_C

> 用 C 从零手写的 epoll HTTP 服务器。轮子自己造，协议自己解，内存自己管。

一个用于学习与娱乐目的的单人项目：从零实现一个 HTTP/1.1 服务器，
不依赖任何第三方运行时，只借助少量工具与灵感来源。

[English](README.md) | [简体中文](README.zh-CN.md)

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![Build](https://img.shields.io/badge/build-nob-orange.svg)](https://github.com/tsoding/nob.h)

---

## 性能

在一台普通开发机上的本地回环压测结果（`wrk`，10k 请求）：

```
Requests/sec:  26812
Latency (p50): 17.8 ms
Latency (p99): 60.2 ms
```

> [!NOTE]
> 这是一个学习项目，性能并非首要目标。当前实现仍有明显优化空间
> （`TCP_NODELAY`、合并 `send` 调用、减少 `epoll_ctl` 频率），但已经比未优化的
> Python Web 框架快出不少。

--- 

## 特性

- **事件驱动架构** — 基于 Linux `epoll` 的 ET（边沿触发）事件循环，单线程
- **自定义内存分配器** — `ping_arena` 提供轻量级区域分配
- **常见数据结构自带** — 很累......
- **可控的调试系统** — 基于位标志与日志级别的运行时可开关调试输出
- **单文件构建** — 使用 [nob.h](https://github.com/tsoding/nob.h) 驱动编译，无需 CMake / Makefile
- 但是：只支持GET方法，只能用于传输本地文件，一般用于个人博客较为合适
--- 

## 架构

```
include/
├── ds/         # 手写数据结构（bitmap / hash_table / linklist / ping_arena ...）
├── net/        # epoll 事件循环、连接、TCP 服务器
├── other/      # debug、全局时间、素数表
├── protocol/   # HTTP 协议实现 + picohttpparser 封装
├── route/      # 路由表与匹配
└── service/    # 业务处理层（占位，可扩展）
```

---

## 快速开始

### 依赖

- Linux（需要 `epoll`、`sendfile`）
- GCC / Clang，C23
- nob.h，如果没有请下载一个然后放在同名文件下即可，让 `nob_build.c` 识别到即可，就一个单 head 头文件......

### 构建

项目使用 `nob.h` 驱动编译：

```bash
gcc nob_build.c -o nob_build
mkdir build
./build/nob_build main test/tcp_server/test_connection_full.c && ./build/main
```

### 运行

```bash
./build/main
```

默认监听端口 `8080`，静态资源目录为 `static/`。

### 访问

```bash
curl http://127.0.0.1:8080/
```

---

## 项目结构

```
PingServer_C/
├── nob_build.c            # 构建脚本
├── include/               # 头文件（与 src/ 一一对应）
├── src/
│   ├── ds/                # 数据结构实现
│   ├── net/               # 网络层
│   ├── protocol/          # 协议层
│   ├── route/             # 路由层
│   ├── service/           # 业务层（预留）
│   ├── other/             # 工具（debug / time）
│   └── main.c             # 入口
├── static/                # 静态资源
├── test/                  # 单元测试与示例
├── tools/                 # makeheaders 等辅助工具
└── docs/                  # 设计文档（epoll 状态机图等）
```

---

## 调试

调试系统基于两级位标志控制：**功能开关**（flag）+ **日志级别**（level）。

```c
/* 只输出 IPv4 与 MAC 层的日志 */
DEBUG_FLAG_SET(DEBUG_FLAG_IPV4 | DEBUG_FLAG_MAC);

/* 按级别过滤 */
DEBUG_LEVEL_SET(LV_WARN | LV_ERROR);

/* 使用 */
DEBUG(DEBUG_FLAG_IPV4, LV_INFO, "packet received, len=%d\n", len);
```

编译时定义 `PINGNET_DEBUG_ENABLE` 才会保留调试代码，否则所有宏展开为空操作，零开销。
可以在 `nob_build` 中自定义开启什么宏展开选项。

---

## 致谢

本项目的若干设计灵感与工具来自以下开源项目，在此表示诚挚的感谢：

- **Tsoding** — [Arena Allocator](https://github.com/tsoding/arena) 为本项目内存分配器提供了灵感来源
- **Tsoding** — [nob.h](https://github.com/tsoding/nob.h) 提供了无需构建系统的单文件编译方案
- **D. Richard Hipp** — [makeheaders](https://www.hwaci.com/sw/mkhdr/) 用于从头文件生成声明
- **H2O 团队** — [picohttpparser](https://github.com/h2o/picohttpparser) 提供了零拷贝、极小体积的 HTTP 解析器

---

## 许可

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE)。

## 最后
I hate C.
