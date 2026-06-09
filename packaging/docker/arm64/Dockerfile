FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        cmake \
        dpkg-dev \
        file \
        g++-aarch64-linux-gnu \
        gcc-aarch64-linux-gnu \
        libc6-dev-arm64-cross \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY aarch64-linux-gnu.cmake /usr/local/share/cardputer-zero-gba/aarch64-linux-gnu.cmake
COPY package-arm64.sh /usr/local/bin/cardputer-zero-gba-package-arm64
RUN chmod 0755 /usr/local/bin/cardputer-zero-gba-package-arm64

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/cardputer-zero-gba-package-arm64"]
