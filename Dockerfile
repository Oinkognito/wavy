FROM alpine:latest

WORKDIR /app

RUN apk add --no-cache \
    build-base \
    cmake \
    make \
    bash \
    wget \
    boost-dev \
    openssl-dev \
    zstd \
    pkgconf \
    libarchive-dev

COPY . /app

RUN make clean && make init && make server-cert-gen && make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON"

EXPOSE 8080

CMD ["./build/hls_server"]
