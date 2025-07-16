FROM alpine:latest

WORKDIR /app

RUN apk add --no-cache \
    build-base \
    cmake \
    make \
    bash \
    wget \
    curl \
    boost-dev \
    openssl-dev \
    openssl \
    zstd \
    git \
    pkgconf \
    libarchive-dev \
    pulseaudio-dev

RUN git clone --recursive https://github.com/Oinkognito/Wavy

WORKDIR /app/Wavy

RUN make server-cert-gen && make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON -DNO_TBB=ON"

EXPOSE 8080

CMD ["./build/wavy_server"]
