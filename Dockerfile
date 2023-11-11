FROM ubuntu:22.04 as prometheus-cpp-builder

RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y curl git cmake build-essential file zlib1g-dev && rm -rf /var/lib/apt/lists/*

ARG PROMETHEUS_CPP_VERSION=v1.1.0

RUN git clone https://github.com/jupp0r/prometheus-cpp /tmp/prometheus-cpp && \
    cd /tmp/prometheus-cpp && \
    git checkout ${PROMETHEUS_CPP_VERSION} && \
    git submodule init && \
    git submodule update && \
    mkdir build && \
    cd build && \
    cmake -DCPACK_GENERATOR=DEB -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=ON .. && \
    cmake --build . --target package --parallel $(nproc) && \
    mv prometheus-cpp_*.deb /prometheus-cpp.deb && \
    cd - && \
    rm -rf /tmp/prometheus-cpp

FROM ghcr.io/usa-reddragon/trunk-recorder:soapysdrplay3@sha256:017ca06b165f45bd00b31b987f1fb61c905d5a3a79de0d869bbc84d4e4912130

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
