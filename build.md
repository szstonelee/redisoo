# How Build (example of MySQL)

I changed the build from make to cmake for Redisoo. It makes the build process easier. (Reference: [old build](build_old.md))

## What You need

### Build Tools

1. gcc, g++
2. make & cmake (cmake version above 3.15)
3. autoconf (sometimes for Jemaloc issue for Linux)
4. grpc

You can install some tools in Linux by 
```
sudo apt-get update && sudo apt-get install build-essential
```
But the cmake version is not the latest. You can check your cmake version
```
cmake --version
```
If the cmake version is lower than 3.15 or there are no cmake, please follow the following steps.

### upgrade/install cmake to the latest version in Linux

Because cmake needs the latest version, you can do something as follow to upgrade your cmake
1. Remove old version of cmake
```
sudo apt purge --auto-remove cmake
```
2. Obtain a copy of the signing key
```
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
```
3. Add the repository to your sources list

a. For Ubuntu Focal Fossa (20.04)
```
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'     
```
b. For Ubuntu Bionic Beaver (18.04)
```
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
```
c. For Ubuntu Xenial Xerus (16.04)
```
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main'
```
4. Update and install
```
sudo apt update
sudo apt install cmake
```
[Reference is here](https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line)

You can check the version of cmake by 'cmake --version'.

As of June 2020, the latest cmake version is 3.17.

### Grpc (&protobuf)

[Referenc here](https://grpc.io/docs/languages/cpp/quickstart/)

I list the commands for you

Linux
```
$ sudo apt install -y build-essential autoconf libtool pkg-config
```
Mac
```
brew install autoconf automake libtool pkg-config
```
Then
```
cd
git clone --recurse-submodules -b v1.28.1 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
make -j
sudo make install
popd
```

### Libraries

1. MySQL C Client Library
2. SOCI (if useing MySQL, you need install MySQL C Client Library first)
3. Jemaloc (only for Linux, already in Makefile of Redisoo)

The following sections will guide you to install the libraries.

## How install MySQL C Client Library

### Reference Website

https://dev.mysql.com/downloads/c-api/ (This is for 8.0, but I did not use 8.0)

Old downloads are here : https://downloads.mysql.com/archives/c-c/

Or you can use the following simpler ways

### Mac OS

```
brew install mysql-connector-c
```
### Linux (my VM is Ubuntu 20.04 18.04, 16.04 LTS of [Multipass](https://github.com/canonical/multipass))

```
sudo apt-get update
sudo apt-get install libmysqlclient-dev
```

### check result of the installation of MySQL C Client Library
sqlclient library needs be in somewhere of /usr folder (e.g. my Linux is /usr/lib/x86_64-linux-gnu/libmysqlclient.a)
```
sudo find /usr -name libmysqlclient.a (For my Linux VM, the search result is /usr/lib/x86_64-linux-gnu/libmysqlclient.a)
sudo find /usr -name libmysqlclient.dylib (For Mac OS)
sudo find /usr -name libmysqlclient.so (For Linux)
```
Actually, the static lib, libmysqlclient.a, is useless, because SOCI use dynamic lib loading

## How build SOCI

I have included SOCI source code files (and modified one) in the deps folder.

So just go on to build the Redisoo.

If you want to know more about SOCI, check the following links:

http://soci.sourceforge.net/

http://soci.sourceforge.net/doc/master/

https://github.com/SOCI/soci


## How build Redisoo

Build Redisoo is easy

```
cd
git clone https://github.com/szstonelee/redisoo.git redisoo
cd redisoo
mkdir build
cd build
cmake ..
make
```

After cmake .., you can see the following screen to check whether MySQL library is OK in your machine
```
-- MySQL - SOCI backend for MySQL
-- SOCI_MYSQL                               = ON
-- SOCI_MYSQL_TARGET                        = soci_mysql
-- SOCI_MYSQL_OUTPUT_NAME                   = soci_mysql
-- SOCI_MYSQL_COMPILE_DEFINITIONS           = SOCI_ABI_VERSION="4.0" HAVE_DL=1
-- SOCI_MYSQL_INCLUDE_DIRECTORIES           = /home/ubuntu/redisoo/build/deps/soci /home/ubuntu/redisoo/deps/soci/include /home/ubuntu/redisoo/build/deps/soci/include /home/ubuntu/redisoo/deps/soci/include/private /home/ubuntu/redisoo/deps/soci/include/private /usr/include/mysql
``` 

If success, there are only some warning but no error messages. You can check
```
cd lib
ls -all redisoo
```
If you can see **redisoo** file, the build is finished!

NOTE: The executable file 'redisoo' is in the sub-folder 'lib' of the 'build' folder 

because redisoo need the dynamic library such as libsoci_core.* and libsoci_mysql.* (for MySQL).

And you can try to run it
```
./redisoo
```

If something wrong, you need 

1. Gurarentee the MySQL Client & SOCI building are successful, because in this user case, Redisoo is based on the two libraries.

For other databases, I have not tested, but you can use the similiar way.

2. Sometimes, Jemalloc can not build successfully

if you see the following error message
```
In file included from server.h:64:0,
                 from backend.c:1:
zmalloc.h:50:10: fatal error: jemalloc/jemalloc.h: No such file or directory
 #include <jemalloc/jemalloc.h>
          ^~~~~~~~~~~~~~~~~~~~~
compilation terminated.
Makefile:311: recipe for target 'backend.o' failed
make[1]: *** [backend.o] Error 1
```

You need do 
```
cd
cd redisoo
cd deps
cd jemalloc
autoreconf -i
cd ..
make jemalloc
```

Then do the above build process again.

## If you want build the gRpc Golang sample server

first, install the go tools to the 1.13 version

For Linux
```
sudo apt-get update
sudo apt-get -y upgrade
wget https://dl.google.com/go/go1.13.3.linux-amd64.tar.gz
sudo tar -xvf go1.13.3.linux-amd64.tar.gz
sudo mv go /usr/local
export GOROOT=/usr/local/go
export PATH=$GOPATH/bin:$GOROOT/bin:$PATH
cd
cd redisoo
cd go_sample_server
go build server.go
```

If build the go source successfully, you can see server in the go_sample_server folder

run it 
```
cd
cd redisoo
cd go_sample_server
./server
```
this grpc sample server will listen in the localhost:40051 port.

[And you can check how to use it.](use.md)


## At last, how use Redisoo

[Click here for more](use.md)


