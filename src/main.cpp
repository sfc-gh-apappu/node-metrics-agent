#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include "cpu_metrics.hpp"
#include "gpu_metrics.hpp"
#include "prometheus.hpp"

namespace {

constexpr int kListenPort = 9100;
constexpr const char* kListenAddr = "0.0.0.0";
constexpr size_t kTopProcessCount = 100;

std::string BuildHttpResponse(int status_code, const std::string& body) {
  std::ostringstream out;
  if (status_code == 200) {
    out << "HTTP/1.1 200 OK\r\n";
  } else {
    out << "HTTP/1.1 404 Not Found\r\n";
  }
  out << "Content-Type: text/plain; version=0.0.4\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n\r\n";
  out << body;
  return out.str();
}

void ServeForever() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Socket error: " << std::strerror(errno) << std::endl;
    return;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kListenPort);
  addr.sin_addr.s_addr = inet_addr(kListenAddr);

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "Bind error: " << std::strerror(errno) << std::endl;
    close(server_fd);
    return;
  }

  if (listen(server_fd, 16) < 0) {
    std::cerr << "Listen error: " << std::strerror(errno) << std::endl;
    close(server_fd);
    return;
  }

  std::cout << "Listening on " << kListenAddr << ":" << kListenPort << std::endl;

  while (true) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr),
                           &client_len);
    if (client_fd < 0) {
      std::cerr << "Accept error: " << std::strerror(errno) << std::endl;
      continue;
    }

    char buffer[4096];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
      close(client_fd);
      continue;
    }
    buffer[bytes] = '\0';

    std::string request(buffer);
    bool is_metrics = request.rfind("GET /metrics", 0) == 0;
    std::string body;
    int status = 200;
    if (is_metrics) {
      CpuMetrics cpu_metrics = CollectCpuMetrics();
      CpuTopProcesses cpu_processes = CollectTopCpuProcesses(kTopProcessCount);
      std::vector<GpuMetrics> gpu_metrics = CollectGpuMetrics();
      body = FormatPrometheus(cpu_metrics, cpu_processes, gpu_metrics);
    } else {
      status = 404;
      body = "not found\n";
    }

    std::string response = BuildHttpResponse(status, body);
    send(client_fd, response.c_str(), response.size(), 0);
    close(client_fd);
  }
}

}  // namespace

int main() {
  InitializeGpuSubsystem();
  ServeForever();
  ShutdownGpuSubsystem();

  return 0;
}
