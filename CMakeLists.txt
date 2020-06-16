# because gRPC need cmake 3.15
cmake_minimum_required(VERSION 3.15)

project(redisoo)

add_subdirectory(deps)

#execute_process(
#  COMMAND sh -c ./mkreleasehdr.sh
#  WORKING_DIRECTORY ${REDIS_ROOT}/src/
#)


set(SRC_SERVER
  src/adlist.c src/quicklist.c src/ae.c src/anet.c src/dict.c src/server.c 
  src/sds.c src/zmalloc.c src/lzf_c.c src/lzf_d.c src/pqsort.c src/zipmap.c
  src/sha1.c src/ziplist.c src/release.c src/networking.c src/util.c
  src/object.c src/db.c src/replication.c src/rdb.c 
  src/t_string.c src/t_list.c src/t_set.c src/t_zset.c src/t_hash.c 
  src/config.c src/aof.c src/pubsub.c src/multi.c src/debug.c src/sort.c 
  src/intset.c src/syncio.c src/cluster.c src/crc16.c src/endianconv.c 
  src/slowlog.c src/scripting.c src/bio.c src/rio.c src/rand.c src/memtest.c 
  src/crcspeed.c src/crc64.c src/bitops.c src/sentinel.c src/notify.c 
  src/setproctitle.c src/blocked.c src/hyperloglog.c src/latency.c src/sparkline.c 
  src/redis-check-rdb.c src/redis-check-aof.c src/geo.c src/lazyfree.c 
  src/module.c src/evict.c src/expire.c src/geohash.c src/geohash_helper.c
  src/childinfo.c src/defrag.c src/siphash.c src/rax.c src/t_stream.c 
  src/listpack.c src/localtime.c src/lolwut.c src/lolwut5.c src/lolwut6.c 
  src/acl.c src/gopher.c src/tracking.c src/connection.c src/tls.c 
  src/sha256.c src/timeout.c src/setcpuaffinity.c
  src/backend.c src/redisoo.cc
)

add_executable(redisoo ${SRC_SERVER})

add_custom_target(release_header COMMAND sh -c ./mkreleasehdr.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src)
add_dependencies(redisoo release_header)

target_include_directories(redisoo PUBLIC src deps/lua/src deps/hiredis deps/linenoise deps/soci/include ${CMAKE_BINARY_DIR}/deps/soci/include)

set_property(TARGET redisoo PROPERTY C_STANDARD 99)
set_property(TARGET redisoo PROPERTY CXX_STANDARD 11)
set_property(TARGET redisoo PROPERTY CMAKE_CXX_STANDARD 11)
set_property(TARGET redisoo PROPERTY CXX_STANDARD_REQUIRED ON)

add_custom_target(soci_core_static_lib COMMAND make WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/deps/soci)

add_library(soci STATIC IMPORTED)
add_dependencies(soci soci_core_static_lib)
set_target_properties(soci PROPERTIES IMPORTED_LOCATION lib/libsoci_core.a)

link_directories(deps/hiredis/ deps/linenoise/ deps/lua/)

target_link_libraries(redisoo
    PRIVATE pthread
    PRIVATE m
    PRIVATE lua_static
    PRIVATE linenoise
    PRIVATE hiredis
    PRIVATE soci
)


