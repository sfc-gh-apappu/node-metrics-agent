#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct CpuMetrics {
  double load_1m = 0.0;
  double cpu_utilization = 0.0;
  double cpu_pressure_avg10 = 0.0;
  double memory_pressure_avg10 = 0.0;
  unsigned long long mem_total_bytes = 0;
  unsigned long long mem_available_bytes = 0;
};

struct CpuProcessMetrics {
  int pid = 0;
  std::string name;
  double cpu_time_seconds = 0.0;
  unsigned long long rss_bytes = 0;
};

struct CpuTopProcesses {
  std::vector<CpuProcessMetrics> processes;
};

CpuMetrics CollectCpuMetrics();
CpuTopProcesses CollectTopCpuProcesses(size_t max_processes);

// Convenience accessors for individual metrics.
double GetCpuLoad1m();
unsigned long long GetNodeMemoryTotalBytes();
unsigned long long GetNodeMemoryAvailableBytes();
CpuTopProcesses GetCpuProcessCpuSecondsTotal(size_t max_processes);
CpuTopProcesses GetCpuProcessRssBytes(size_t max_processes);

// Returns overall node health score in range [0, 10].
double GetNodeHealthScore();
double ComputeNodeHealthScore(const CpuMetrics& metrics);
