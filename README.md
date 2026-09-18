# PingServer_C

> An epoll-based HTTP server written from scratch in C. Own wheels, own parser, own memory.

A solo project built for learning and fun: implementing an HTTP/1.1 server from the ground up,
with no third-party runtime dependencies — only a handful of tools and sources of inspiration.

[English](README.md) | [简体中文](README.zh-CN.md)

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![Build](https://img.shields.io/badge/build-nob-orange.svg)](https://github.com/tsoding/nob.h)

---

## Features

- **Event-driven architecture** — Linux `epoll` with edge-triggered mode, single-threaded
- **Custom memory allocator** — `ping_arena` provides lightweight region allocation
- **Batteries-included data structures** — and yes, it was exhausting...
- **Controllable debug system** — runtime-toggleable output via bit flags and log levels
- **Single-file build** — driven by [nob.h](https://github.com/tsoding/nob.h), no CMake / Makefile needed

---

## Architecture

```
include/
├── ds/         # Hand-written data structures (bitmap / hash_table / linklist / ping_arena ...)
├── net/        # epoll event loop, connection, TCP server
├── other/      # debug, global time, prime table
├── protocol/   # HTTP protocol + picohttpparser wrapper
├── route/      # Routing table and matching
└── service/    # Service layer (placeholder, extensible)
```

---

## Getting Started

### Requirements

- Linux (`epoll`, `sendfile`)
- GCC / Clang with C23 support
- `nob.h` — if you don't have it, just download it and drop it next to `nob_build.c`. It's a single header file, that's it...

### Build

The project is compiled via `nob.h`:

```bash
gcc nob_build.c -o nob_build
mkdir build
./build/nob_build main test/tcp_server/test_connection_full.c && ./build/main
```

### Run

```bash
./build/main
```

Listens on port `8080` by default, serving static assets from `static/`.

### Access

```bash
curl http://127.0.0.1:8080/
```

---

## Project Layout

```
PingServer_C/
├── nob_build.c            # Build script
├── include/               # Headers (one-to-one with src/)
├── src/
│   ├── ds/                # Data structures
│   ├── net/               # Network layer
│   ├── protocol/          # Protocol layer
│   ├── route/             # Routing layer
│   ├── service/           # Service layer (reserved)
│   ├── other/             # Utilities (debug / time)
│   └── main.c             # Entry point
├── static/                # Static assets
├── test/                  # Unit tests and examples
├── tools/                 # makeheaders and other utilities
└── docs/                  # Design docs (epoll state machine diagram, etc.)
```

---

## Debugging

The debug system is controlled by two levels of bit flags: **feature flags** and **log levels**.

```c
/* Only log from the IPv4 and MAC layers */
DEBUG_FLAG_SET(DEBUG_FLAG_IPV4 | DEBUG_FLAG_MAC);

/* Filter by level */
DEBUG_LEVEL_SET(LV_WARN | LV_ERROR);

/* Usage */
DEBUG(DEBUG_FLAG_IPV4, LV_INFO, "packet received, len=%d\n", len);
```

Defining `PINGNET_DEBUG_ENABLE` at compile time keeps the debug code; otherwise every macro
expands to a no-op with zero overhead. You can customize which macros get expanded in `nob_build`.

---

## Performance

Local loopback benchmark on an ordinary dev machine (`wrk`, 10k requests):

```
Requests/sec:  26812
Latency (p50): 17.8 ms
Latency (p99): 60.2 ms
```

> [!NOTE]
> This is a learning project — performance is not the primary goal. There is still plenty of
> room for improvement (`TCP_NODELAY`, coalescing `send` calls, reducing `epoll_ctl` frequency),
> but it already outperforms a non-optimized Python web framework by a comfortable margin.

---

## Acknowledgements

Several design ideas and tools in this project come from the following open-source projects.
Sincere thanks to their authors:

- **Tsoding** — [Arena Allocator](https://github.com/tsoding/arena) inspired this project's memory allocator
- **Tsoding** — [nob.h](https://github.com/tsoding/nob.h) provides the build-system-free single-file compilation flow
- **D. Richard Hipp** — [makeheaders](https://www.hwaci.com/sw/mkhdr/) is used to generate declarations from headers
- **The H2O team** — [picohttpparser](https://github.com/h2o/picohttpparser) provides a zero-copy, minimal HTTP parser

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.
