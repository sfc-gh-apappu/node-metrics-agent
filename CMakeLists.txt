cmake_minimum_required(VERSION 3.16)
project(gpu_node_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(node-metrics-agent
  src/main.cpp
  src/cpu_metrics.cpp
  src/gpu_metrics.cpp
  src/prometheus.cpp
  src/util.cpp
)

target_include_directories(node-metrics-agent PRIVATE include)

option(USE_NVML "Enable NVML GPU metrics" ON)
if(USE_NVML)
  target_compile_definitions(node-metrics-agent PRIVATE USE_NVML)
  target_link_libraries(node-metrics-agent PRIVATE nvidia-ml)
endif()
