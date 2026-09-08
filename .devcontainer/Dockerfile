FROM mcr.microsoft.com/devcontainers/base:ubuntu

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        clang-format cppcheck \
        python3-pip \
    && pip3 install --no-cache-dir --break-system-packages pre-commit \
    && rm -rf /var/lib/apt/lists/*