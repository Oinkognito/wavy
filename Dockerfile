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

# generate OpenSSL self signed cert and private key (Wavy Server WON'T run without it!)
RUN openssl req -x509 -newkey rsa:4096 \
        -keyout /app/server.key \
        -out /app/server.crt \
        -days 365 -nodes \
        -subj "/CN=localhost" && \
    chmod 600 /app/server.key

RUN make clean && make init && make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON"

EXPOSE 8080

CMD ["./build/hls_server"]
