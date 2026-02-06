# Node Metrics Agent (PoC)

Lightweight C++ agent that exposes basic CPU/GPU metrics via Prometheus text
format on `0.0.0.0:9100/metrics`. Designed to run as a DaemonSet on GPU nodes,
but can run in CPU-only mode for local development.

## Features
- CPU metrics (works on Linux and macOS):
  - `cpu_load_1m`
  - `node_memory_total_bytes`
  - `node_memory_available_bytes`
  - `cpu_process_cpu_seconds_total{pid,name}`
  - `cpu_process_rss_bytes{pid,name}`
- GPU metrics (NVML, Linux + NVIDIA drivers):
  - `gpu_utilization_percent`
  - `gpu_memory_used_bytes`, `gpu_memory_total_bytes`
  - `gpu_temperature_celsius`
  - `gpu_power_draw_watts`
  - `gpu_process_memory_bytes{gpu_index, pid}`

## Project layout
- `src/main.cpp`: HTTP server and wiring.
- `src/cpu_metrics.cpp`: CPU collection (Linux `/proc`, macOS sysctl/mach).
- `src/gpu_metrics.cpp`: NVML init/shutdown and GPU/process metrics.
- `src/prometheus.cpp`: Prometheus text formatting.
- `src/util.cpp`: shared helpers.
- `include/`: public headers.
- `deploy/daemonset.yaml`: Kubernetes DaemonSet manifest.
- `config/prometheus.yml`: local Prometheus scrape config.

## Quick start

### macOS (CPU-only)
```bash
cmake -S . -B build -DUSE_NVML=OFF
cmake --build build
```

Run:
```bash
./build/node-metrics-agent
```

Start Prometheus (local):
```bash
prometheus --config.file=./config/prometheus.yml
```

### Linux GPU host (NVML + Docker)
```bash
docker build -t node-metrics-agent:latest .
docker run --rm -p 9100:9100 --gpus all \
  -v /dev:/dev:ro -v /proc:/proc:ro \
  node-metrics-agent:latest
```

## Build

### CPU-only (no NVML required)
```bash
cmake -S . -B build -DUSE_NVML=OFF
cmake --build build
```

### GPU-enabled (Linux + NVIDIA drivers)
```bash
cmake -S . -B build -DUSE_NVML=ON
cmake --build build
```

## Kubernetes
Apply the DaemonSet:
```bash
kubectl apply -f deploy/daemonset.yaml
```

## Prometheus (local)
Use the repo-provided config:
```bash
prometheus --config.file=./config/prometheus.yml
```

Start/stop via Homebrew service (if installed with brew):
```bash
brew services start prometheus
brew services stop prometheus
```

Example queries:
- `cpu_load_1m`
- `node_memory_available_bytes / node_memory_total_bytes`
- `topk(5, cpu_process_rss_bytes)`
- `topk(5, cpu_process_cpu_seconds_total)`
- `topk(5, 100 * cpu_process_rss_bytes / scalar(node_memory_total_bytes))`
- `topk(5, rate(cpu_process_cpu_seconds_total[1m]))`

## Notes
- GPU metrics require NVML and access to `/dev/nvidia*`.
- Container/pod mapping from `/proc/<pid>/cgroup` is a stub (see TODO).
