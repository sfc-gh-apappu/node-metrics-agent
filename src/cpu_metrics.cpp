#include "cpu_metrics.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#endif

#include "util.hpp"

namespace {

double Clamp(double value, double min_value, double max_value) {
  return std::min(std::max(value, min_value), max_value);
}

double ParsePressureAvg10(const std::string& content) {
  const std::string needle = "avg10=";
  size_t pos = content.find(needle);
  if (pos == std::string::npos) {
    return 0.0;
  }
  pos += needle.size();
  size_t end = content.find(' ', pos);
  std::string value =
      content.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
  try {
    return std::stod(value);
  } catch (const std::exception&) {
    return 0.0;
  }
}

double GetCpuCoreCount() {
#ifdef __linux__
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  return cores > 0 ? static_cast<double>(cores) : 1.0;
#elif defined(__APPLE__)
  uint32_t cores = 0;
  size_t size = sizeof(cores);
  if (sysctlbyname("hw.logicalcpu", &cores, &size, nullptr, 0) == 0 &&
      cores > 0) {
    return static_cast<double>(cores);
  }
  return 1.0;
#else
  return 1.0;
#endif
}

}  // namespace

CpuMetrics CollectCpuMetrics() {
  CpuMetrics metrics;

#ifdef __linux__
  std::string loadavg = ReadFile("/proc/loadavg");
  if (!loadavg.empty()) {
    std::istringstream stream(loadavg);
    stream >> metrics.load_1m;
  }

  std::string stat = ReadFile("/proc/stat");
  if (!stat.empty()) {
    std::istringstream stream(stat);
    std::string cpu_label;
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;
    unsigned long long guest = 0;
    unsigned long long guest_nice = 0;
    stream >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >>
        softirq >> steal >> guest >> guest_nice;
    if (cpu_label == "cpu") {
      const unsigned long long idle_all = idle + iowait;
      const unsigned long long non_idle =
          user + nice + system + irq + softirq + steal;
      const unsigned long long total = idle_all + non_idle;
      static unsigned long long prev_total = 0;
      static unsigned long long prev_idle = 0;
      if (prev_total != 0 && total > prev_total && idle_all >= prev_idle) {
        const unsigned long long total_delta = total - prev_total;
        const unsigned long long idle_delta = idle_all - prev_idle;
        if (total_delta > 0) {
          metrics.cpu_utilization =
              static_cast<double>(total_delta - idle_delta) /
              static_cast<double>(total_delta);
        }
      }
      prev_total = total;
      prev_idle = idle_all;
    }
  }

  metrics.cpu_pressure_avg10 = ParsePressureAvg10(ReadFile("/proc/pressure/cpu"));
  metrics.memory_pressure_avg10 =
      ParsePressureAvg10(ReadFile("/proc/pressure/memory"));

  std::string meminfo = ReadFile("/proc/meminfo");
  if (!meminfo.empty()) {
    std::istringstream stream(meminfo);
    std::string key;
    unsigned long long value_kb = 0;
    std::string unit;
    while (stream >> key >> value_kb >> unit) {
      if (key == "MemTotal:") {
        metrics.mem_total_bytes = value_kb * 1024ULL;
      } else if (key == "MemAvailable:") {
        metrics.mem_available_bytes = value_kb * 1024ULL;
      }
    }
  }

  if (metrics.mem_total_bytes == 0 && metrics.mem_available_bytes == 0 &&
      metrics.load_1m == 0.0) {
    std::cerr << "CPU metrics unavailable; /proc not readable?" << std::endl;
  }

  return metrics;
#elif defined(__APPLE__)
  double loadavg_values[3];
  if (getloadavg(loadavg_values, 3) != -1) {
    metrics.load_1m = loadavg_values[0];
  }

  uint64_t mem_total = 0;
  size_t mem_total_size = sizeof(mem_total);
  if (sysctlbyname("hw.memsize", &mem_total, &mem_total_size, nullptr, 0) == 0) {
    metrics.mem_total_bytes = mem_total;
  }

  vm_statistics64_data_t vm_stats;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_stats),
                        &count) == KERN_SUCCESS) {
    const uint64_t page_size = static_cast<uint64_t>(getpagesize());
    // Approximate "available" as free + inactive pages.
    metrics.mem_available_bytes =
        (vm_stats.free_count + vm_stats.inactive_count) * page_size;
  }

  if (metrics.mem_total_bytes == 0 && metrics.mem_available_bytes == 0 &&
      metrics.load_1m == 0.0) {
    std::cerr << "CPU metrics unavailable; sysctl/mach calls failed?"
              << std::endl;
  }

  return metrics;
#else
  std::cerr << "CPU metrics unavailable; unsupported platform" << std::endl;
  return metrics;
#endif
}

CpuTopProcesses CollectTopCpuProcesses(size_t max_processes) {
  CpuTopProcesses result;

#ifdef __linux__
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  const auto time_exhausted = [&deadline]() {
    return std::chrono::steady_clock::now() > deadline;
  };

  DIR* proc_dir = opendir("/proc");
  if (!proc_dir) {
    std::cerr << "Failed to open /proc: " << std::strerror(errno) << std::endl;
    return result;
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  const long ticks_per_second = sysconf(_SC_CLK_TCK);
  struct dirent* entry = nullptr;
  while ((entry = readdir(proc_dir)) != nullptr) {
    if (time_exhausted()) {
      break;
    }
    if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
      continue;
    }

    const char* name = entry->d_name;
    if (!name || name[0] < '0' || name[0] > '9') {
      continue;
    }

    int pid = std::atoi(name);
    if (pid <= 0) {
      continue;
    }

    std::string stat = ReadFile("/proc/" + std::to_string(pid) + "/stat");
    if (stat.empty()) {
      continue;
    }

    size_t open_paren = stat.find('(');
    size_t close_paren = stat.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos ||
        close_paren <= open_paren) {
      continue;
    }

    CpuProcessMetrics proc;
    proc.pid = pid;
    proc.name = stat.substr(open_paren + 1, close_paren - open_paren - 1);

    std::string rest = stat.substr(close_paren + 2);
    std::istringstream fields(rest);
    std::vector<std::string> tokens;
    std::string token;
    while (fields >> token) {
      tokens.push_back(token);
    }

    if (tokens.size() < 22) {
      continue;
    }

    unsigned long long utime = std::strtoull(tokens[11].c_str(), nullptr, 10);
    unsigned long long stime = std::strtoull(tokens[12].c_str(), nullptr, 10);
    long rss_pages = std::strtol(tokens[21].c_str(), nullptr, 10);

    const unsigned long long total_ticks = utime + stime;
    if (ticks_per_second > 0) {
      proc.cpu_time_seconds =
          static_cast<double>(total_ticks) / ticks_per_second;
    }
    proc.rss_bytes =
        rss_pages > 0 ? static_cast<unsigned long long>(rss_pages) *
                            static_cast<unsigned long long>(page_size)
                      : 0;

    result.processes.push_back(std::move(proc));
  }

  closedir(proc_dir);

  std::sort(result.processes.begin(), result.processes.end(),
            [](const CpuProcessMetrics& a, const CpuProcessMetrics& b) {
              return a.cpu_time_seconds > b.cpu_time_seconds;
            });

  if (result.processes.size() > max_processes) {
    result.processes.resize(max_processes);
  }

  return result;
#elif defined(__APPLE__)
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  const auto time_exhausted = [&deadline]() {
    return std::chrono::steady_clock::now() > deadline;
  };

  int buffer_size = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  if (buffer_size <= 0) {
    std::cerr << "Failed to list processes via proc_listpids" << std::endl;
    return result;
  }

  std::vector<pid_t> pids(static_cast<size_t>(buffer_size) / sizeof(pid_t));
  buffer_size = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                              static_cast<int>(pids.size() * sizeof(pid_t)));
  if (buffer_size <= 0) {
    std::cerr << "Failed to populate PID list via proc_listpids" << std::endl;
    return result;
  }

  for (pid_t pid : pids) {
    if (time_exhausted()) {
      break;
    }
    if (pid <= 0) {
      continue;
    }

    char name_buffer[PROC_PIDPATHINFO_MAXSIZE] = {0};
    if (proc_name(pid, name_buffer, sizeof(name_buffer)) <= 0) {
      continue;
    }

    struct proc_taskinfo taskinfo;
    int bytes = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskinfo,
                             static_cast<int>(sizeof(taskinfo)));
    if (bytes != sizeof(taskinfo)) {
      continue;
    }

    CpuProcessMetrics proc;
    proc.pid = static_cast<int>(pid);
    proc.name = name_buffer;
    proc.rss_bytes = taskinfo.pti_resident_size;
    const uint64_t total_ns =
        taskinfo.pti_total_user + taskinfo.pti_total_system;
    proc.cpu_time_seconds = static_cast<double>(total_ns) / 1e9;

    result.processes.push_back(std::move(proc));
  }

  std::sort(result.processes.begin(), result.processes.end(),
            [](const CpuProcessMetrics& a, const CpuProcessMetrics& b) {
              return a.cpu_time_seconds > b.cpu_time_seconds;
            });

  if (result.processes.size() > max_processes) {
    result.processes.resize(max_processes);
  }

  return result;
#else
  std::cerr << "Top process metrics unavailable on this platform" << std::endl;
  return result;
#endif
}

double GetCpuLoad1m() {
  return CollectCpuMetrics().load_1m;
}

unsigned long long GetNodeMemoryTotalBytes() {
  return CollectCpuMetrics().mem_total_bytes;
}

unsigned long long GetNodeMemoryAvailableBytes() {
  return CollectCpuMetrics().mem_available_bytes;
}

CpuTopProcesses GetCpuProcessCpuSecondsTotal(size_t max_processes) {
  return CollectTopCpuProcesses(max_processes);
}

CpuTopProcesses GetCpuProcessRssBytes(size_t max_processes) {
  return CollectTopCpuProcesses(max_processes);
}

double ComputeNodeHealthScore(const CpuMetrics& metrics) {
  double cpu_util_score = Clamp(1.0 - metrics.cpu_utilization, 0.0, 1.0);

  const double cores = GetCpuCoreCount();
  const double cpu_load_score =
      Clamp(1.0 - (metrics.load_1m / cores), 0.0, 1.0);

  double mem_score = 0.0;
  if (metrics.mem_total_bytes > 0) {
    mem_score = static_cast<double>(metrics.mem_available_bytes) /
                static_cast<double>(metrics.mem_total_bytes);
  }
  mem_score = Clamp(mem_score, 0.0, 1.0);

  const double cpu_pressure_penalty =
      Clamp(metrics.cpu_pressure_avg10 / 100.0, 0.0, 1.0);
  const double memory_pressure_penalty =
      Clamp(metrics.memory_pressure_avg10 / 100.0, 0.0, 1.0);

  const double cpu_score =
      0.6 * cpu_util_score + 0.4 * cpu_load_score;
  const double weighted =
      0.5 * cpu_score + 0.3 * mem_score +
      0.1 * (1.0 - cpu_pressure_penalty) +
      0.1 * (1.0 - memory_pressure_penalty);
  return Clamp(weighted, 0.0, 1.0) * 10.0;
}

double GetNodeHealthScore() {
  return ComputeNodeHealthScore(CollectCpuMetrics());
}
