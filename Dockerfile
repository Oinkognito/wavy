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
    ca-certificates \
    pkgconf \
    libarchive-dev \
    lmdb-dev \
    && update-ca-certificates

COPY . /app/Wavy

WORKDIR /app/Wavy/external/crow
RUN rm -rf build && mkdir -p build && cd build && cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF && make install

WORKDIR /app/Wavy

RUN make clean && make server-cert-gen && make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON -DNO_TBB=ON"

EXPOSE 8080

CMD ["./build/wavy_server"]
