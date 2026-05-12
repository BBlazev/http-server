# Benchmarks

## v1 — epoll + thread pool 5/2026
Date: [12/5/2026]
Build: g++ -O2
Hardware: VirtualBox Debian VM, [hardware_concurrency] cores

### wrk -t4 -c100 -d30s http://localhost:8080/index.html
Req/sec: 850
Latency: avg 135ms, p99 ~470ms

### wrk -t4 -c1000 -d60s http://localhost:8080/index.html  
Req/sec: 835
Latency: avg 1.16s, p99 ~2s
Errors: 0 read
Sustained: 50,000+ requests