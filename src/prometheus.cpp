#include "prometheus.hpp"

#include <string>

namespace {

constexpr bool kIncludeHelpType = false;

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

template <typename T>
void AppendNumber(std::string* out, T value) {
  out->append(std::to_string(value));
}

}  // namespace

void FormatPrometheus(const CpuMetrics& cpu_metrics,
                      const CpuTopProcesses& cpu_processes,
                      const std::vector<GpuMetrics>& gpu_metrics,
                      std::string* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (kIncludeHelpType) {
    out->append("# HELP cpu_load_1m 1-minute system load average.\n");
    out->append("# TYPE cpu_load_1m gauge\n");
    out->append("# HELP node_cpu_utilization_ratio CPU utilization ratio (0-1).\n");
    out->append("# TYPE node_cpu_utilization_ratio gauge\n");
    out->append(
        "# HELP node_cpu_pressure_avg10 CPU pressure avg10 (0-100).\n");
    out->append("# TYPE node_cpu_pressure_avg10 gauge\n");
    out->append(
        "# HELP node_memory_pressure_avg10 Memory pressure avg10 (0-100).\n");
    out->append("# TYPE node_memory_pressure_avg10 gauge\n");
    out->append("# HELP node_memory_total_bytes System memory total in bytes.\n");
    out->append("# TYPE node_memory_total_bytes gauge\n");
    out->append(
        "# HELP node_memory_available_bytes System memory available in bytes.\n");
    out->append("# TYPE node_memory_available_bytes gauge\n");
    out->append("# HELP node_health_score Overall node health score (0-10).\n");
    out->append("# TYPE node_health_score gauge\n");
    out->append("# HELP cpu_process_cpu_seconds_total Process CPU time in seconds.\n");
    out->append("# TYPE cpu_process_cpu_seconds_total counter\n");
    out->append("# HELP cpu_process_rss_bytes Process resident memory in bytes.\n");
    out->append("# TYPE cpu_process_rss_bytes gauge\n");
    out->append("# HELP gpu_utilization_percent GPU utilization percentage.\n");
    out->append("# TYPE gpu_utilization_percent gauge\n");
    out->append("# HELP gpu_memory_used_bytes GPU memory used in bytes.\n");
    out->append("# TYPE gpu_memory_used_bytes gauge\n");
    out->append("# HELP gpu_memory_total_bytes GPU memory total in bytes.\n");
    out->append("# TYPE gpu_memory_total_bytes gauge\n");
    out->append("# HELP gpu_temperature_celsius GPU temperature in Celsius.\n");
    out->append("# TYPE gpu_temperature_celsius gauge\n");
    out->append("# HELP gpu_power_draw_watts GPU power draw in watts.\n");
    out->append("# TYPE gpu_power_draw_watts gauge\n");
    out->append("# HELP gpu_process_memory_bytes GPU memory used per process.\n");
    out->append("# TYPE gpu_process_memory_bytes gauge\n");
  }

  out->append("cpu_load_1m ");
  AppendNumber(out, cpu_metrics.load_1m);
  out->push_back('\n');
  out->append("node_cpu_utilization_ratio ");
  AppendNumber(out, cpu_metrics.cpu_utilization);
  out->push_back('\n');
  out->append("node_cpu_pressure_avg10 ");
  AppendNumber(out, cpu_metrics.cpu_pressure_avg10);
  out->push_back('\n');
  out->append("node_memory_pressure_avg10 ");
  AppendNumber(out, cpu_metrics.memory_pressure_avg10);
  out->push_back('\n');
  out->append("node_memory_total_bytes ");
  AppendNumber(out, cpu_metrics.mem_total_bytes);
  out->push_back('\n');
  out->append("node_memory_available_bytes ");
  AppendNumber(out, cpu_metrics.mem_available_bytes);
  out->push_back('\n');
  out->append("node_health_score ");
  AppendNumber(out, ComputeNodeHealthScore(cpu_metrics));
  out->push_back('\n');

  for (const auto& proc : cpu_processes.processes) {
    out->append("cpu_process_cpu_seconds_total{pid=\"");
    AppendNumber(out, proc.pid);
    out->append("\",name=\"");
    out->append(EscapeLabelValue(proc.name));
    out->append("\"} ");
    AppendNumber(out, proc.cpu_time_seconds);
    out->push_back('\n');

    out->append("cpu_process_rss_bytes{pid=\"");
    AppendNumber(out, proc.pid);
    out->append("\",name=\"");
    out->append(EscapeLabelValue(proc.name));
    out->append("\"} ");
    AppendNumber(out, proc.rss_bytes);
    out->push_back('\n');
  }

  for (const auto& gpu : gpu_metrics) {
    out->append("gpu_utilization_percent{gpu_index=\"");
    AppendNumber(out, gpu.index);
    out->append("\"} ");
    AppendNumber(out, gpu.utilization_gpu_percent);
    out->push_back('\n');

    out->append("gpu_memory_used_bytes{gpu_index=\"");
    AppendNumber(out, gpu.index);
    out->append("\"} ");
    AppendNumber(out, gpu.memory_used_bytes);
    out->push_back('\n');

    out->append("gpu_memory_total_bytes{gpu_index=\"");
    AppendNumber(out, gpu.index);
    out->append("\"} ");
    AppendNumber(out, gpu.memory_total_bytes);
    out->push_back('\n');

    out->append("gpu_temperature_celsius{gpu_index=\"");
    AppendNumber(out, gpu.index);
    out->append("\"} ");
    AppendNumber(out, gpu.temperature_c);
    out->push_back('\n');

    if (gpu.power_available) {
      out->append("gpu_power_draw_watts{gpu_index=\"");
      AppendNumber(out, gpu.index);
      out->append("\"} ");
      AppendNumber(out, gpu.power_watts);
      out->push_back('\n');
    }

    for (const auto& proc : gpu.processes) {
      out->append("gpu_process_memory_bytes{gpu_index=\"");
      AppendNumber(out, gpu.index);
      out->append("\",pid=\"");
      AppendNumber(out, proc.pid);
      out->append("\"} ");
      AppendNumber(out, proc.used_gpu_memory_bytes);
      out->push_back('\n');
    }
  }
}
