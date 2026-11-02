
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build-docker --parallel

CMD ["./build-docker/tests"]
