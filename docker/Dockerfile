FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    make=4.3-4.1build2 \
    gcc=4:13.2.0-7ubuntu1 \
    libc6-dev=2.39-0ubuntu8.9 \
    python3=3.12.3-0ubuntu2.1 \
    gcc-arm-none-eabi=15:13.2.rel1-2 \
    binutils-arm-none-eabi=2.42-1ubuntu1+23 \
    libnewlib-arm-none-eabi=4.4.0.20231231-2 \
    ca-certificates=20260601~24.04.1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
