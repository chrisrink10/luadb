cmake_minimum_required(VERSION 3.1)
project(luadb)
include(ExternalProject)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/")

# Specific compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUA_COMPAT_5_2 -DLUA_USE_APICHECK")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -g") #-O2 -DNDEBUG
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-format-extra-args -Wno-unused-function")

if (${UNIX})
    find_library(LIB_READLINE readline)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUADB_USE_READLINE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUADB_USE_GETOPT")
endif(${UNIX})

# LuaDB native files
set(SOURCE_FILES src/luadb.h
                 src/json.c
                 src/main.c
                 src/state.c)

# CCAN JSON library files
set(JSON_LIB_SRCS src/deps/json/json.c)

# Lua core files
set (LUA_CORE_SRCS src/deps/lua/lapi.c
                   src/deps/lua/lcode.c
                   src/deps/lua/lctype.c
                   src/deps/lua/ldebug.c
                   src/deps/lua/ldo.c
                   src/deps/lua/ldump.c
                   src/deps/lua/lfunc.c
                   src/deps/lua/lgc.c
                   src/deps/lua/llex.c
                   src/deps/lua/lmem.c
                   src/deps/lua/lobject.c
                   src/deps/lua/lopcodes.c
                   src/deps/lua/lparser.c
                   src/deps/lua/lstate.c
                   src/deps/lua/lstring.c
                   src/deps/lua/ltable.c
                   src/deps/lua/ltm.c
                   src/deps/lua/lundump.c
                   src/deps/lua/lvm.c
                   src/deps/lua/lzio.c)

# Lua library files
set (LUA_LIB_SRCS src/deps/lua/lauxlib.c
                  src/deps/lua/lbaselib.c
                  src/deps/lua/lbitlib.c
                  src/deps/lua/lcorolib.c
                  src/deps/lua/ldblib.c
                  src/deps/lua/liolib.c
                  src/deps/lua/lmathlib.c
                  src/deps/lua/loslib.c
                  src/deps/lua/ltablib.c
                  src/deps/lua/lstrlib.c
                  src/deps/lua/lutf8lib.c
                  src/deps/lua/loadlib.c
                  src/deps/lua/linit.c)

# LMDB library files
set(LMDB_LIB_SRCS src/deps/lmdb/lmdb.h
                  src/deps/lmdb/mdb.c
                  src/deps/lmdb/midl.c)

add_executable(luadb ${SOURCE_FILES}
                     ${LUA_CORE_SRCS}
                     ${LUA_LIB_SRCS}
                     ${LMDB_LIB_SRCS}
                     ${JSON_LIB_SRCS})

target_link_libraries(luadb ${LIB_READLINE})
