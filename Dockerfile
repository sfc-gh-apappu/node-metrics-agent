FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ARG USE_NVML=OFF
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    && if [ "${USE_NVML}" = "ON" ]; then apt-get install -y --no-install-recommends libnvidia-ml-dev; fi \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

RUN cmake -S . -B build -DUSE_NVML=${USE_NVML} \
    && cmake --build build --config Release

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/node-metrics-agent /usr/local/bin/node-metrics-agent

ENTRYPOINT ["/usr/local/bin/node-metrics-agent"]
