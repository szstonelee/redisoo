FROM ubuntu:18.04 AS build

WORKDIR /build_redisoo

RUN apt-get update && apt-get -y install \
      build-essential \
      autoconf \
      libtool \
      pkg-config \
      wget \
      software-properties-common \
      git 

# for CMake version over 3.15
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | \
    gpg --dearmor - | \
    tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
    apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main' && \
    apt-get -y install cmake

# gRPC, Notice make -j 1 for small memory (8G) machine
RUN git clone --recurse-submodules -b v1.28.0 https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../.. && \
    make -j 1 && \
    make install

# MySQL C library
RUN apt-get -y install libmysqlclient-dev

# build Redisoo
RUN cd /build_redisoo && \
    git clone https://github.com/szstonelee/redisoo.git redisoo && \
    cd redisoo && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

FROM ubuntu:18.04

WORKDIR /redisoo

RUN apt-get update && apt-get -y install libmysqlclient-dev

COPY --from=build /build_redisoo/redisoo/build/lib .

