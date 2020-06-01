# How Build (example of MySQL)

The build of Redisoo is very complicated, because it depends on many libraries. 

And these libraries are subtle.

## What You need before build

### Build Tools

1. gcc, g++
2. make & cmake
3. autoconf (sometimes for Jemaloc issue for Linux)

### Libraries

1. MySQL C Client Library
2. SOCI (if useing MySQL, you need install MySQL C Client Library first)
3. Jemaloc (only for Linux, already in Makefile of Redisoo)

## How install MySQL C Client Library

### Reference Website

https://dev.mysql.com/downloads/c-api/ (This is for 8.0, but I did not use 8.0)

Old downloads are here : https://downloads.mysql.com/archives/c-c/

Or you can use the following simpler ways

### Mac OS

```
brew install mysql-connector-c
```
### Linux (my VM is Ubuntu 18.04 LTS of [Multipass](https://github.com/canonical/multipass))

```
sudo apt-get update
sudo apt-get install libmysqlclient-dev
```

### check result of the build of MySQL C Client Library
sqlclient library need be there
```
sudo find /usr -name libmysqlclient.a (For my Linux VM, the search result is /usr/lib/x86_64-linux-gnu/libmysqlclient.a)
sudo find /usr -name libmysqlclient.dylib (For Mac OS)
sudo find /usr -name libmysqlclient.so (For Linux)
```
Actually, the static lib, libmysqlclient.a, is useless, because SOCI use dynamic lib loading

You can check the redisoo Makefile in src direcectory, there is no word about mysql!!!

## How build SOCI

### Reference Website

http://soci.sourceforge.net/

http://soci.sourceforge.net/doc/master/

https://github.com/SOCI/soci

### Build Steps

```
cd
git clone https://github.com/SOCI/soci.git soci
cd soci
mkdir build
cd build
cmake -G "Unix Makefiles" -DWITH_BOOST=OFF -DWITH_ORACLE=OFF -DSOCI_CXX11=ON -DWITH_MYSQL=ON ../
make
sudo make install
```

Some result shows as follow when finish the SOCI buiding

For cmake
```
-- Configuring SOCI:
-- SOCI_VERSION                             = 4.0.0
-- SOCI_ABI_VERSION                         = 4.0
-- SOCI_SHARED                              = ON
-- SOCI_STATIC                              = ON
-- SOCI_TESTS                               = ON
-- SOCI_ASAN                                = OFF
-- SOCI_CXX11                               = ON
-- LIB_SUFFIX                               = 64

-- MySQL:
-- Performing Test HAVE_MYSQL_OPT_EMBEDDED_CONNECTION
-- Performing Test HAVE_MYSQL_OPT_EMBEDDED_CONNECTION - Failed
-- Found MySQL: /usr/include/mysql, /usr/lib/x86_64-linux-gnu/libmysqlclient.so
-- MySQL Embedded not found.
-- MYSQL_INCLUDE_DIR                        = /usr/include/mysql
-- MYSQL_LIBRARIES                          = /usr/lib/x86_64-linux-gnu/libmysqlclient.so

-- MySQL - SOCI backend for MySQL
-- SOCI_MYSQL                               = ON
-- SOCI_MYSQL_TARGET                        = soci_mysql
-- SOCI_MYSQL_OUTPUT_NAME                   = soci_mysql
-- SOCI_MYSQL_COMPILE_DEFINITIONS           = SOCI_ABI_VERSION="4.0" HAVE_DL=1
-- SOCI_MYSQL_INCLUDE_DIRECTORIES           = /home/ubuntu/soci/build /home/ubuntu/soci/include /home/ubuntu/soci/build/include /home/ubuntu/soci/include/private /home/ubuntu/soci/include/private /usr/include/mysql
```
For make
```
Scanning dependencies of target soci_mysql
[ 80%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/blob.cpp.o
[ 81%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/common.cpp.o
[ 82%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/factory.cpp.o
[ 84%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/row-id.cpp.o
[ 85%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/session.cpp.o
[ 86%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/standard-into-type.cpp.o
[ 87%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/standard-use-type.cpp.o
[ 88%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/statement.cpp.o
[ 89%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/vector-into-type.cpp.o
[ 90%] Building CXX object src/backends/mysql/CMakeFiles/soci_mysql.dir/vector-use-type.cpp.o
[ 91%] Linking CXX shared library ../../../lib/libsoci_mysql.so
[ 91%] Built target soci_mysql
```

### check result of the build of SOCI 

#### find the library
1. soci_core
```
sudo find /usr -name libsoci_core.a (For my Linux VM, it is /usr/local/lib64/libsoci_core.a)
sudo find /usr -name libsoci_core.so  (For Linux)
sudo find /usr -name libsoci_core.dylib (For Mac OS)
```
2. soci_mysql
```
sudo find /usr -name libsoci_mysql.a (My Linux VM, /usr/local/lib64/libsoci_mysql.a)
sudo find /usr -name libsoci_mysql.so (For Linux)
sudo find /usr -name libsoci_mysql.dylib (For Mac OS)
```

The static library is useless, but you can check whether the building of SOCI is successful.

#### check the *ISSUE* of the shared library In Linux

Because SOCI use dynamic loading of library, so there is an issue with the libsoci_mysql.so.

```
ldd /usr/local/lib64/libsoci_mysql.so
```

If you see the following result, it is an isssue. 

```
libsoci_core.so.4.0 => not found
```

Though Redisoo can start, but it will give the wrong message of **Failed to find shared library for backend xxx**

The whole result of ldd /usr/local/lib64/libsoci_mysql.so is like these with issue
NOTE: the address could be different for different machine and different compiler.
```
	linux-vdso.so.1 (0x00007ffc59776000)
	libsoci_core.so.4.0 => not found
	libmysqlclient.so.20 => /usr/lib/x86_64-linux-gnu/libmysqlclient.so.20 (0x00007f7a26a94000)
	libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007f7a26875000)
	libstdc++.so.6 => /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f7a264ec000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f7a262d4000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f7a25ee3000)
	libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007f7a25cdf000)
	libz.so.1 => /lib/x86_64-linux-gnu/libz.so.1 (0x00007f7a25ac2000)
	libssl.so.1.1 => /usr/lib/x86_64-linux-gnu/libssl.so.1.1 (0x00007f7a25835000)
	libcrypto.so.1.1 => /usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 (0x00007f7a2536a000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f7a27293000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f7a24fcc000)
```

#### How fix "Failed to find shared library for backend xxx"

When you use the above way to check the 'not found' issue, it must resul to 'Failed to find shared library for backend xxx'

Suppose find libsoci_core.so in /usr/local/lib64
then 
```
cd
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64
sudo ldconfig
cd soci
rm -rf build
mkdir build
cd build
cmake -G "Unix Makefiles" -DWITH_BOOST=OFF -DWITH_ORACLE=OFF -DSOCI_CXX11=ON -DWITH_MYSQL=ON ../
make
sudo make install
```

And check it again

```
ldd /usr/local/lib64/libsoci_mysql.so
```
You will find something like the following (No "not found" for libsoci_core)
```
libsoci_core.so.4.0 => /usr/local/lib64/libsoci_core.so.4.0 (0x00007fea48111000)
```
NOTE: the address could be different for different machine and different compiler.

You fix the problem of 'not found' isuse. 

But it does not mean you absolutelly fix it, maybe you need patch for patch, please check the following.

#### Patch for patch

Sometimes, the above way can not work even when you check libsoci_mysql and not find "not found".

This time, you need to do a copy for the libsoci_mysql.so and libsoci_core.so 

```
sudo cp build/lib/libsoci_core.so.4.0.0 /usr/local/lib64/.
sudo cp build/lib/libsoci_mysql.so.4.0.0 /usr/local/lib64/.
```
NOTE: copy the version file, not the link symbolic files of ibsoci_mysql.so and libsoci_core.so.
You can cd build/lib, and ls -all to find the real files (not the symbolic files)

The source files is in the build/lib of moci, the dest directory is the find folder in /usr.

I do not know why, maybe the cache or bug of Linux???

#### debug code for SOCI

You can add some debug code to check the SOCI issue

in the source file of SOCI, soci/src/core/backend-loader.cpp

```
void do_register_backend()
```
replace 
```
if (0 == h)
{
  throw soci_error("Failed to find shared library for backend " + name);
}
```
with
```
    if (0 == h)
    {
        std::string err_msg;
        if (shared_object.empty()) {
            err_msg += "shared_object is empty! ";
            err_msg += "LIBNAME(name) = ";
            err_msg += LIBNAME(name);
            for (std::size_t i = 0; i != search_paths_.size(); ++i) {
                err_msg += ", search_path_";
                err_msg += std::to_string(i);
                err_msg += " = ";
                err_msg += search_paths_[i];
                err_msg += "; ";
            }
            std::string const file_name(search_paths_[1] + "/" + LIBNAME(name));
            err_msg += ", try dlopen file_name = ";
            err_msg += file_name;
            soci_handler_t h_test = DLOPEN(file_name.c_str());
            if (h_test == 0) {
                err_msg += ", h_test == 0";
                err_msg += ", dlerror = ";
                err_msg += dlerror();
            } else {
                err_msg += ", h_test != 0";
            }
        } else {
            err_msg += "shared_object.c_str() = ";
            err_msg += shared_object;
        }

        // throw soci_error("Failed to find shared library for backend " + name);
        throw soci_error("Failed to find shared library for backend = " + name + ", err_msg = " + err_msg);
    }
```
then rebuild soci and Redisoo, and run redisoo for the debug error message

## How build Redisoo (and possible fix an issue of Jemalloc for Linux)

Build Redisoo is easy

```
cd
git clone https://github.com/szstonelee/redisoo.git redisoo
cd redisoo
make
cd src
./redis-server
```

If something wrong, you need 

1. Gurarentee the MySQL Client & SOCI building are successful, because Redisoo is based on these two libraries.

For other databases, I have not tried, but you can use the similiar way.

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

Then 
```
cd
cd redisoo
make
cd src
./redis-server
```

## At last, how use Redisoo

[Click here for more](use.md)


