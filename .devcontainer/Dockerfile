FROM mcr.microsoft.com/devcontainers/base:ubuntu

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates curl \
        clang-format cppcheck \
        gcc-arm-none-eabi libnewlib-arm-none-eabi \
        gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
        qemu-system-arm qemu-user gdb-multiarch \
        python3-pip \
    && pip3 install --no-cache-dir --break-system-packages --ignore-installed packaging pre-commit gcovr \
    && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg -o /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" > /etc/apt/sources.list.d/github-cli.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends gh \
    && rm -rf /var/lib/apt/lists/*