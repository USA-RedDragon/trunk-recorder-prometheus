FROM ubuntu:22.04@sha256:a2bbdf79d03f6d4171bbd6ebb4782f6229375951987d3427ec3d9373f9841cd7 as prometheus-cpp-builder

RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y curl git cmake build-essential file zlib1g-dev && rm -rf /var/lib/apt/lists/*

# renovate: datasource=github-tags depName=jupp0r/prometheus-cpp
ARG PROMETHEUS_CPP_VERSION=v1.2.1

RUN git clone https://github.com/jupp0r/prometheus-cpp -b ${PROMETHEUS_CPP_VERSION} /tmp/prometheus-cpp && \
    cd /tmp/prometheus-cpp && \
    git submodule init && \
    git submodule update && \
    mkdir build && \
    cd build && \
    cmake -DCPACK_GENERATOR=DEB -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=ON .. && \
    cmake --build . --target package --parallel $(nproc) && \
    mv prometheus-cpp_*.deb /prometheus-cpp.deb && \
    cd - && \
    rm -rf /tmp/prometheus-cpp

FROM ghcr.io/robotastic/trunk-recorder:edge@sha256:2429ae8d7d00c107472ee5468bb8914e99b232b2fdebafe51564b0f75649ffc0

COPY --from=prometheus-cpp-builder /prometheus-cpp.deb /tmp/prometheus-cpp.deb
RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y /tmp/prometheus-cpp.deb && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src/trunk-recorder-prometheus

COPY . .

WORKDIR /src/trunk-recorder-prometheus/build

RUN cmake .. && make install

RUN rm -rf /src/trunk-recorder-prometheus

WORKDIR /app
