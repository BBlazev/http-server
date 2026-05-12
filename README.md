# HTTP Server (C++17)

A from-scratch HTTP/1.1 server using Linux epoll and a custom thread pool.

## What it does

Serves static files over HTTP/1.1. One thread runs an epoll-based event loop that monitors all connections. When a request is complete, it gets dispatched to a worker thread pool for parsing and response. Workers never block on network I/O.

## Performance

Tested on VirtualBox Debian VM, single-file static response (201 bytes).

| Concurrency | Duration | Req/sec | Avg Latency | Errors |
|-------------|----------|---------|-------------|--------|
| 100         | 10s      | 728     | 135ms       | 0      |
| 1000        | 10s      | 854     | 927ms       | 0 read |
| 1000        | 60s      | 835     | 1.16s       | 0 read |


## Correctness

- Clean ThreadSanitizer run under load (no data races)
- Clean AddressSanitizer run under load (no leaks)
- 50,000 requests over 60 seconds with zero read errors

## Build

```
make           # release (-O2)
make tsan      # ThreadSanitizer
make asan      # AddressSanitizer
```

## Run

```
./server
```

Listens on port 8080. Serves files from `./www/`.

## Known limitations

- No HTTP keep-alive
- No HTTPS
- No timeout on slow/silent clients 
- No graceful SIGINT handling
- Static file serving GET only
- Blocking writes - need EPOLLOUT for larger writes

## Stack

C++17, linux