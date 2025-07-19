FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    libstdc++-12-dev \
    libstdc++-12-doc \
    libstdc++-12-pic \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN mkdir build && cd build && cmake .. && make 

CMD ["./build/tradeit"]]