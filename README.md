# Node Metrics Agent (PoC)

Lightweight C++ agent that runs on each node and exposes CPU/GPU metrics in
Prometheus text format on `0.0.0.0:9100/metrics`, plus `healthz`/`readyz`
endpoints. Designed to run as a DaemonSet on GPU nodes, but can run in CPU-only
mode for local development.

## How it works
- Run one agent per node (DaemonSet).
- Prometheus scrapes `/metrics` from each agent.
- `/healthz` and `/readyz` are for Kubernetes probes.
- `node_health_score` summarizes CPU, memory, and pressure signals.

## Features
- CPU metrics (works on Linux and macOS):
  - `cpu_load_1m`
  - `node_cpu_utilization_ratio`
  - `node_cpu_pressure_avg10` (Linux PSI)
  - `node_memory_pressure_avg10` (Linux PSI)
  - `node_memory_total_bytes`
  - `node_memory_available_bytes`
  - `node_health_score`
  - `cpu_process_cpu_seconds_total{pid,name}`
  - `cpu_process_rss_bytes{pid,name}`
  - Node health score (0-10) derived from CPU, memory, and pressure signals
- GPU metrics (NVML, Linux + NVIDIA drivers):
  - `gpu_utilization_percent`
  - `gpu_memory_used_bytes`, `gpu_memory_total_bytes`
  - `gpu_temperature_celsius`
  - `gpu_power_draw_watts`
  - `gpu_process_memory_bytes{gpu_index, pid}`
- Endpoints:
  - `/metrics` (Prometheus scrape target)
  - `/healthz` (liveness)
  - `/readyz` (readiness)

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
docker build -t node-metrics-agent:latest --build-arg USE_NVML=ON .
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

### Docker build defaults
Docker builds are CPU-only by default. For GPU builds, pass:
```bash
docker build -t node-metrics-agent:latest --build-arg USE_NVML=ON .
```

## Kubernetes
Apply the DaemonSet:
```bash
kubectl apply -f deploy/daemonset.yaml
```

## Kubernetes (kind simulation with 3 nodes)
Create a 3-node kind cluster:
```bash
kind create cluster --name node-metrics --config deploy/kind/cluster-3node.yaml
```

Build and load the image into the kind cluster:
```bash
docker build -t node-metrics-agent:latest .
kind load docker-image node-metrics-agent:latest --name node-metrics
```

Deploy the CPU-only DaemonSet (schedules on all nodes):
```bash
kubectl apply -f deploy/daemonset-cpu.yaml
```

Deploy Prometheus in-cluster:
```bash
kubectl apply -f deploy/prometheus.yaml
```

Port-forward Prometheus and open the UI:
```bash
kubectl -n monitoring port-forward svc/prometheus 9090:9090
```

PromQL examples are listed below.

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

## PromQL examples
- `cpu_load_1m`
- `node_health_score`
- `node_cpu_utilization_ratio`
- `node_cpu_pressure_avg10`
- `node_memory_pressure_avg10`
- `node_memory_available_bytes / node_memory_total_bytes`
- `sort_desc(node_health_score)`
- `avg by (node) (node_memory_available_bytes / node_memory_total_bytes)`
- `sum by (node) (rate(cpu_process_cpu_seconds_total[1m]))`
- `topk(5, cpu_process_rss_bytes)`
- `topk(5, cpu_process_cpu_seconds_total)`
- `topk(5, 100 * cpu_process_rss_bytes / scalar(node_memory_total_bytes))`
- `topk(5, rate(cpu_process_cpu_seconds_total[1m]))`

## Notes
- GPU metrics require NVML and access to `/dev/nvidia*`.
- Container/pod mapping from `/proc/<pid>/cgroup` is a stub (see TODO).
- Linux PSI metrics require `/proc/pressure/*` (available on most modern kernels).
- Metrics are cached and refreshed every 2s to keep scrape latency low.

## Node health score
`GetNodeHealthScore()` (exposed as `node_health_score`) returns a 0-10 score
based on available headroom:
- CPU score: weighted mix of utilization and load average.
- Memory score: `node_memory_available_bytes / node_memory_total_bytes`,
  clamped to [0, 1].
- Pressure penalties: Linux PSI `avg10` for CPU and memory.
- Overall: weighted blend of CPU, memory, and pressure signals.

Intuition: higher load or lower available memory reduces the score, and the
weights favor CPU responsiveness while still penalizing low memory headroom.
