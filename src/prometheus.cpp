#include "prometheus.hpp"

#include <sstream>

namespace {

std::string EscapeLabelValue(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

}  // namespace

std::string FormatPrometheus(const CpuMetrics& cpu_metrics,
                             const CpuTopProcesses& cpu_processes,
                             const std::vector<GpuMetrics>& gpu_metrics) {
  std::ostringstream out;
  out << "# HELP cpu_load_1m 1-minute system load average.\n";
  out << "# TYPE cpu_load_1m gauge\n";
  out << "# HELP node_memory_total_bytes System memory total in bytes.\n";
  out << "# TYPE node_memory_total_bytes gauge\n";
  out << "# HELP node_memory_available_bytes System memory available in bytes.\n";
  out << "# TYPE node_memory_available_bytes gauge\n";
  out << "# HELP node_health_score Overall node health score (0-10).\n";
  out << "# TYPE node_health_score gauge\n";
  out << "# HELP cpu_process_cpu_seconds_total Process CPU time in seconds.\n";
  out << "# TYPE cpu_process_cpu_seconds_total counter\n";
  out << "# HELP cpu_process_rss_bytes Process resident memory in bytes.\n";
  out << "# TYPE cpu_process_rss_bytes gauge\n";
  out << "# HELP gpu_utilization_percent GPU utilization percentage.\n";
  out << "# TYPE gpu_utilization_percent gauge\n";
  out << "# HELP gpu_memory_used_bytes GPU memory used in bytes.\n";
  out << "# TYPE gpu_memory_used_bytes gauge\n";
  out << "# HELP gpu_memory_total_bytes GPU memory total in bytes.\n";
  out << "# TYPE gpu_memory_total_bytes gauge\n";
  out << "# HELP gpu_temperature_celsius GPU temperature in Celsius.\n";
  out << "# TYPE gpu_temperature_celsius gauge\n";
  out << "# HELP gpu_power_draw_watts GPU power draw in watts.\n";
  out << "# TYPE gpu_power_draw_watts gauge\n";
  out << "# HELP gpu_process_memory_bytes GPU memory used per process.\n";
  out << "# TYPE gpu_process_memory_bytes gauge\n";

  out << "cpu_load_1m " << cpu_metrics.load_1m << "\n";
  out << "node_memory_total_bytes " << cpu_metrics.mem_total_bytes << "\n";
  out << "node_memory_available_bytes " << cpu_metrics.mem_available_bytes
      << "\n";
  out << "node_health_score " << ComputeNodeHealthScore(cpu_metrics) << "\n";

  for (const auto& proc : cpu_processes.processes) {
    out << "cpu_process_cpu_seconds_total{pid=\"" << proc.pid << "\",name=\""
        << EscapeLabelValue(proc.name) << "\"} " << proc.cpu_time_seconds
        << "\n";
    out << "cpu_process_rss_bytes{pid=\"" << proc.pid << "\",name=\""
        << EscapeLabelValue(proc.name) << "\"} " << proc.rss_bytes << "\n";
  }

  for (const auto& gpu : gpu_metrics) {
    out << "gpu_utilization_percent{gpu_index=\"" << gpu.index << "\"} "
        << gpu.utilization_gpu_percent << "\n";
    out << "gpu_memory_used_bytes{gpu_index=\"" << gpu.index << "\"} "
        << gpu.memory_used_bytes << "\n";
    out << "gpu_memory_total_bytes{gpu_index=\"" << gpu.index << "\"} "
        << gpu.memory_total_bytes << "\n";
    out << "gpu_temperature_celsius{gpu_index=\"" << gpu.index << "\"} "
        << gpu.temperature_c << "\n";
    if (gpu.power_available) {
      out << "gpu_power_draw_watts{gpu_index=\"" << gpu.index << "\"} "
          << gpu.power_watts << "\n";
    }
    for (const auto& proc : gpu.processes) {
      out << "gpu_process_memory_bytes{gpu_index=\"" << gpu.index
          << "\",pid=\"" << proc.pid << "\"} " << proc.used_gpu_memory_bytes
          << "\n";
    }
  }

  return out.str();
}
