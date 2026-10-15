FROM mcr.microsoft.com/devcontainers/base:ubuntu

# One layer per tool group, ordered from least to most likely to change, so
# editing a package list only rebuilds its own layer and the ones after it.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates curl \
        clang-format cppcheck \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi libnewlib-arm-none-eabi \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-pip \
    && pip3 install --no-cache-dir --break-system-packages --ignore-installed packaging pre-commit gcovr \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg -o /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" > /etc/apt/sources.list.d/github-cli.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends gh \
    && rm -rf /var/lib/apt/lists/*

# Simulation tooling, the most recently added and so the most likely to change.
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
        qemu-system-arm qemu-user gdb-multiarch \
    && rm -rf /var/lib/apt/lists/*
