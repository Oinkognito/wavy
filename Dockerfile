FROM alpine:latest

WORKDIR /app

RUN apk add --no-cache \
    build-base \
    cmake \
    make \
    bash \
    wget \
    curl \
    asio-dev \
    openssl-dev \
    openssl \
    zstd \
    git \
    pkgconf \
    libarchive-dev

RUN git clone --recursive https://github.com/Oinkognito/Wavy

WORKDIR /app/Wavy/external/crow
RUN mkdir -p build && cd build && cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF && make install

WORKDIR /app/Wavy

RUN make server-cert-gen && make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON -DNO_TBB=ON"

EXPOSE 8080

CMD ["./build/wavy_server"]
