FROM ubuntu:22.04@sha256:c7eb020043d8fc2ae0793fb35a37bff1cf33f156d4d4b12ccc7f3ef8706c38b1 as prometheus-cpp-builder

RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y curl git cmake build-essential file zlib1g-dev && rm -rf /var/lib/apt/lists/*

# renovate: datasource=github-tags depName=jupp0r/prometheus-cpp
ARG PROMETHEUS_CPP_VERSION=v1.3.0

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

FROM ghcr.io/robotastic/trunk-recorder:edge@sha256:d0299591441159e4c3844668975821a76aa7295340c0bb3b10d0945d67240bb7

COPY --from=prometheus-cpp-builder /prometheus-cpp.deb /tmp/prometheus-cpp.deb
RUN apt update && export DEBIAN_FRONTEND=noninteractive && \
    apt install -y /tmp/prometheus-cpp.deb && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src/trunk-recorder-prometheus

COPY . .

WORKDIR /src/trunk-recorder-prometheus/build

RUN mv /etc/gnuradio/conf.d/gnuradio-runtime.conf /tmp/gnuradio-runtime.conf && \
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install --no-install-recommends --no-install-suggests -y \
        git \
        cmake \
        make \
        libssl-dev \
        build-essential \
        gnuradio-dev \
        libuhd-dev \
        libcurl4-openssl-dev \
        libsndfile1-dev && \
    cmake .. && make install && \
    apt-get purge -y git cmake make libssl-dev build-essential gnuradio-dev libuhd-dev libcurl4-openssl-dev libsndfile1-dev && \
    mv /tmp/gnuradio-runtime.conf /etc/gnuradio/conf.d/gnuradio-runtime.conf && \
    rm -rf /var/lib/apt/lists/*

RUN rm -rf /src/trunk-recorder-prometheus

WORKDIR /app
