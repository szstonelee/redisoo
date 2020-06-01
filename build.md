# How Build (example of MySQL)

The build of Redisoo is very complicated, because it depends on many library. 

And these libraries are trivial.

## What You need before build

### Build Tools

1. gcc, g++
2. make & cmake
3. autoreconf(sometimes for Jemaloc for Linux)

### Libraries

1. MySQL C Client Library
2. SOCI (if useing MySQL, you need install MySQL C Client Library first)

## How install MySQL C Client Library

### Reference Website

https://dev.mysql.com/downloads/c-api/ (This is for 8.0, but I did not use 8.0)

Old download is here : https://downloads.mysql.com/archives/c-c/

Or you can use the following simpler ways

### Mac OS

```
brew install mysql-connector-c
```
### Linux

```
sudo apt-get update
sudo apt-get install libmysqlclient-dev
```

### check success
sqlclient library need be there
```
sudo find /usr -name libmysqlclient.a
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
git clone https://github.com/SOCI/soci.git soci
cd soci
mkdir build
cd build
cmake -G "Unix Makefiles" -DWITH_BOOST=OFF -DWITH_ORACLE=OFF -DSOCI_CXX11=ON -DWITH_MYSQL=ON ../
make
sudo make install
```

### check 

#### find the library
1. soci_core
```
sudo find /usr -name libsoci_core.a
sudo find /usr -name libsoci_core.so  (For Linux)
sudo find /usr -name libsoci_core.dylib (For Mac OS)
```
2. soci_mysql
```
sudo find /usr -name libsoci_mysql.a
sudo find /usr -name libsoci_mysql.so (For Linux)
sudo find /usr -name libsoci_mysql.dylib (For Mac OS)
```

The static library is useless, but you can check whether the building of the library is successful.

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

#### How fix "Failed to find shared library for backend xxx"

Suppose using find libsoci_core.so in /usr/local/lib64
then 
```
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

I do not know why, maybe the cache of Linux???

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

## How build Redisoo

Build Redisoo is easy

```
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

You need do 
```
cd redisoo
cd deps
cd jemalloc
autoreconf -i
make
```

## Then how use Redisoo

[Click here for more](use.md)


