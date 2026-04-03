# Stage 1: Build
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y build-essential cmake git

WORKDIR /app/source
COPY . .

RUN make clean && make all -j 4 && make test

# Stage 2: Runtime
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y libjson-c5 && rm -rf /var/lib/apt/lists/*

WORKDIR /app/bin

# Binaries are built into bin/ by the Makefile
COPY --from=builder /app/source/bin/ .

ENV PATH="/app/bin:${PATH}"

EXPOSE 8787 8788

CMD ["./gomoku-httpd", "-b", "0.0.0.0:8787", "-a", "8788", "-L", "debug"]
