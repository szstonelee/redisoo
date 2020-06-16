#set(LUA_SRC
#    src/lapi.c src/lcode.c src/ldebug.c src/ldo.c src/ldump.c src/lfunc.c 
#    src/lgc.c src/llex.c src/lmem.c src/lobject.c src/lopcodes.c src/lparser.c 
#    src/lstate.c src/lstring.c src/ltable.c src/ltm.c src/lundump.c src/lvm.c 
#    src/lzio.c src/strbuf.c fpconv.c src/lauxlib.c src/lbaselib.c src/ldblib.c 
#    src/liolib.c src/lmathlib.c src/loslib.c src/ltablib.c src/lstrlib.c src/loadlib.c 
#    src/linit.c src/lua_cjson.c src/lua_struct.c src/lua_cmsgpack.c src/lua_bit.c
#)

#add_library(lua STATIC
#  ${LUA_SRC}
#)

#set(LUA_LIB_SRCS 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lapi.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lcode.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lctype.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldebug.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldo.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldump.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lfunc.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lgc.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/llex.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lmem.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lobject.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lopcodes.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lparser.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lstate.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lstring.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltable.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltm.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lundump.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lvm.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lzio.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lauxlib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lbaselib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lbitlib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lcorolib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldblib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/liolib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lmathlib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/loslib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lstrlib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltablib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/lutf8lib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/loadlib.c 
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/linit.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c)


file(GLOB LUA_LIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
set(LUA_LIB_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(lua_static STATIC ${LUA_LIB_SRCS})
target_include_directories(lua_static PUBLIC ${LUA_LIB_INCLUDE})
set(LUA_DEFINITIONS LUA_USE_POSIX)
if(EMSCRIPTEN)
    unset(LUA_DEFINITIONS)
endif()
target_compile_definitions(lua_static PUBLIC ${LUA_DEFINITIONS})
