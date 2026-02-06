#pragma once

#include <string>
#include <vector>

struct ProcMetrics {
  unsigned int pid = 0;
  unsigned long long used_gpu_memory_bytes = 0;
  std::string cgroup_path;
  std::string container_id;
};

struct GpuMetrics {
  unsigned int index = 0;
  unsigned int utilization_gpu_percent = 0;
  unsigned long long memory_used_bytes = 0;
  unsigned long long memory_total_bytes = 0;
  unsigned int temperature_c = 0;
  bool power_available = false;
  double power_watts = 0.0;
  std::vector<ProcMetrics> processes;
};

void InitializeGpuSubsystem();
void ShutdownGpuSubsystem();
std::vector<GpuMetrics> CollectGpuMetrics();
