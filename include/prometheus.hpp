#pragma once

#include <string>
#include <vector>

#include "cpu_metrics.hpp"
#include "gpu_metrics.hpp"

void FormatPrometheus(const CpuMetrics& cpu_metrics,
                      const CpuTopProcesses& cpu_processes,
                      const std::vector<GpuMetrics>& gpu_metrics,
                      std::string* out);
