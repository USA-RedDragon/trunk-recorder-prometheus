FROM ubuntu:22.04@sha256:6042500cf4b44023ea1894effe7890666b0c5c7871ed83a97c36c76ae560bb9b as prometheus-cpp-builder

RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y curl git cmake build-essential file zlib1g-dev && rm -rf /var/lib/apt/lists/*

# renovate: datasource=github-tags depName=jupp0r/prometheus-cpp
ARG PROMETHEUS_CPP_VERSION=v1.2.0

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

FROM ghcr.io/robotastic/trunk-recorder:edge@sha256:e7b891c4c8f535b28b29c5d6b1cc57896e12d0d39dc3e1bc7d37e2b4bf3097c5

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
