#include "gpu_metrics.hpp"

#include <iostream>
#include <sstream>

#include "util.hpp"

#ifdef USE_NVML
#include <nvml.h>
#endif

namespace {

std::string ExtractContainerIdFromCgroup(const std::string& cgroup_contents) {
  // TODO: Implement robust parsing for cgroup v1/v2 to map to pod/container IDs.
  // As a simple placeholder, return the last path segment that looks non-empty.
  std::istringstream stream(cgroup_contents);
  std::string line;
  std::string candidate;
  while (std::getline(stream, line)) {
    auto pos = line.rfind(':');
    if (pos == std::string::npos) {
      continue;
    }
    std::string path = line.substr(pos + 1);
    if (path.empty()) {
      continue;
    }
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < path.size()) {
      candidate = path.substr(slash + 1);
    } else {
      candidate = path;
    }
  }
  return candidate;
}

}  // namespace

void InitializeGpuSubsystem() {
#ifdef USE_NVML
  nvmlReturn_t nvml_result = nvmlInit_v2();
  if (nvml_result != NVML_SUCCESS) {
    std::cerr << "NVML init failed: " << nvmlErrorString(nvml_result)
              << std::endl;
    std::exit(1);
  }
  std::cout << "NVML initialized" << std::endl;
#else
  std::cout << "NVML disabled; running in CPU-only mode" << std::endl;
#endif
}

void ShutdownGpuSubsystem() {
#ifdef USE_NVML
  nvmlReturn_t nvml_result = nvmlShutdown();
  if (nvml_result != NVML_SUCCESS) {
    std::cerr << "NVML shutdown failed: " << nvmlErrorString(nvml_result)
              << std::endl;
  }
#endif
}

std::vector<GpuMetrics> CollectGpuMetrics() {
  std::vector<GpuMetrics> result;

#ifndef USE_NVML
  std::cerr << "NVML disabled at build time; returning empty GPU metrics"
            << std::endl;
  return result;
#else
  unsigned int device_count = 0;
  nvmlReturn_t nvml_result = nvmlDeviceGetCount_v2(&device_count);
  if (nvml_result != NVML_SUCCESS) {
    std::cerr << "NVML: failed to get device count: "
              << nvmlErrorString(nvml_result) << std::endl;
    return result;
  }

  for (unsigned int i = 0; i < device_count; ++i) {
    nvmlDevice_t device;
    nvml_result = nvmlDeviceGetHandleByIndex_v2(i, &device);
    if (nvml_result != NVML_SUCCESS) {
      std::cerr << "NVML: failed to get device handle for index " << i << ": "
                << nvmlErrorString(nvml_result) << std::endl;
      continue;
    }

    GpuMetrics metrics;
    metrics.index = i;

    nvmlUtilization_t utilization{};
    nvml_result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (nvml_result == NVML_SUCCESS) {
      metrics.utilization_gpu_percent = utilization.gpu;
    }

    nvmlMemory_t memory{};
    nvml_result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (nvml_result == NVML_SUCCESS) {
      metrics.memory_used_bytes = memory.used;
      metrics.memory_total_bytes = memory.total;
    }

    unsigned int temp = 0;
    nvml_result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
    if (nvml_result == NVML_SUCCESS) {
      metrics.temperature_c = temp;
    }

    unsigned int power_mw = 0;
    nvml_result = nvmlDeviceGetPowerUsage(device, &power_mw);
    if (nvml_result == NVML_SUCCESS) {
      metrics.power_available = true;
      metrics.power_watts = static_cast<double>(power_mw) / 1000.0;
    }

    unsigned int process_count = 0;
    nvml_result =
        nvmlDeviceGetComputeRunningProcesses_v2(device, &process_count, nullptr);
    if (nvml_result == NVML_SUCCESS && process_count == 0) {
      result.push_back(metrics);
      continue;
    }

    if (nvml_result != NVML_ERROR_INSUFFICIENT_SIZE &&
        nvml_result != NVML_SUCCESS) {
      std::cerr << "NVML: failed to get process count for GPU " << i << ": "
                << nvmlErrorString(nvml_result) << std::endl;
      result.push_back(metrics);
      continue;
    }

    std::vector<nvmlProcessInfo_t> processes(process_count);
    nvml_result = nvmlDeviceGetComputeRunningProcesses_v2(
        device, &process_count, processes.data());
    if (nvml_result != NVML_SUCCESS) {
      std::cerr << "NVML: failed to get process list for GPU " << i << ": "
                << nvmlErrorString(nvml_result) << std::endl;
      result.push_back(metrics);
      continue;
    }

    for (unsigned int p = 0; p < process_count; ++p) {
      ProcMetrics proc;
      proc.pid = processes[p].pid;
      proc.used_gpu_memory_bytes = processes[p].usedGpuMemory;
      proc.cgroup_path =
          ReadFile("/proc/" + std::to_string(proc.pid) + "/cgroup");
      proc.container_id = ExtractContainerIdFromCgroup(proc.cgroup_path);
      metrics.processes.push_back(std::move(proc));
    }

    result.push_back(std::move(metrics));
  }

  return result;
#endif
}
