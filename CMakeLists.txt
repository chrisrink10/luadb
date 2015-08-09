#############################################################################
# LuaDB :: CMakeLists.txst
#
# CMake directives for building LuaDB
#
# Author:  Chris Rink <chrisrink10@gmail.com>
#
# License: MIT (see LICENSE document at source tree root)
#############################################################################

cmake_minimum_required(VERSION 3.1)
project(luadb)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckPrototypeDefinition)
include(CheckStructHasMember)
include(CheckSymbolExists)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/")

##############################
# Check Capabilities
##############################

#check_prototype_definition(fileno 1 2 3 HAVE_FILENO_PROTO)

check_symbol_exists(fpos_t stdio.h HAVE_FPOS)
check_symbol_exists(socklen_t sys/socket.h HAVE_SOCKLEN)

check_struct_has_member(sockaddr_un sun_len sys/un.h HAVE_SOCKADDR_UN_SUN_LEN)

check_include_files(arpa/inet.h HAVE_ARPA_INET_H)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(getopt.h HAVE_GETOPT_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(netdb.h HAVE_NETDB_H)
check_include_files(netinet/in.h HAVE_NETINET_IN_H)
check_include_files(pthread.h HAVE_PTHREAD)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(sys/param.h HAVE_SYS_PARAM_H)
check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(uuid/uuid.h HAVE_UUID_H)
if(WIN32)
    check_include_files(objbase.h HAVE_OBJBASE_H)
    check_include_files(windows.h HAVE_WINDOWS_H)
endif(WIN32)

check_function_exists(strerror HAVE_STRERROR)

find_library(LIB_READLINE readline)
find_library(LIB_SOCKET socket)
if (LIB_SOCKET)
    set(HAVE_LIBSOCKET 1)
endif(LIB_SOCKET)
find_library(LIB_NSL nsl)
if (LIB_NSL)
    set(HAVE_LIBNSL 1)
endif(LIB_NSL)

##############################
# Build Flags
##############################

# Cache the existing flags
set(LUADB_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

# Specific compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUA_USE_APICHECK")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pthread") #-O2 -DNDEBUG
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-format-extra-args")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")

# FastCGI flags
set(LUADB_FCGI_C_FLAGS "-D_XOPEN_SOURCE -D__USE_MISC")
set(LUADB_FCGI_C_FLAGS "${LUADB_FCGI_C_FLAGS} -Wno-pointer-sign -Wno-unused-function -Wno-unused-variable")
configure_file(${PROJECT_SOURCE_DIR}/src/deps/fcgi/fcgi_config.h.in
               ${PROJECT_SOURCE_DIR}/src/deps/fcgi/fcgi_config.h)

# LuaDB Flags
set(LUADB_C_FLAGS "-Werror -Wno-unused-function")
if (HAVE_UUID_H)
    set(LUADB_C_FLAGS "${LUADB_C_FLAGS} -DLUADB_USE_UUID")
endif(HAVE_UUID_H)

# LMDB Flags
set(LUADB_LMDB_C_FLAGS "-O2 -g -Wno-unused-parameter -Wbad-function-cast -Wuninitialized")

# Specify the GNU99 standard if we're using MINGW
if (MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif(MINGW)

# Test for readline
if(LIB_READLINE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLUADB_USE_READLINE")
endif(LIB_READLINE)

##############################
# Source Files
##############################

# LuaDB native files
set(SOURCE_FILES src/luadb.h
                 src/fcgi.c
                 src/json.c
                 src/lmdb.c
                 src/main.c
                 src/state.c
                 src/util.c
                 src/uuid.c)

##############################
# Create Libraries
##############################

# FastCGI library files
set (FCGI_LIB_SRCS src/deps/fcgi/fcgiapp.c
                   src/deps/fcgi/fastcgi.h
                   src/deps/fcgi/fcgimisc.h
                   src/deps/fcgi/fcgios.h)
if(${WIN32})
    set (FCGI_LIB_SRCS ${FCGI_LIB_SRCS} src/deps/fcgi/os_win32.c)
elseif(${UNIX})
    set (FCGI_LIB_SRCS ${FCGI_LIB_SRCS} src/deps/fcgi/os_unix.c)
endif(${WIN32})
add_library(fastcgi STATIC ${FCGI_LIB_SRCS})
set_target_properties(fastcgi PROPERTIES
    COMPILE_FLAGS "${LUADB_FCGI_C_FLAGS}"
)

# CCAN JSON library files
set(JSON_LIB_SRCS src/deps/json/json.c)
add_library(json STATIC ${JSON_LIB_SRCS})

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
add_library(luacore STATIC ${LUA_CORE_SRCS})

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
add_library(lual STATIC ${LUA_LIB_SRCS})

# LMDB library files
set(LMDB_LIB_SRCS src/deps/lmdb/lmdb.h
                  src/deps/lmdb/mdb.c
                  src/deps/lmdb/midl.c)
add_library(lmdb STATIC ${LMDB_LIB_SRCS})
set_target_properties(lmdb PROPERTIES
    COMPILE_FLAGS "${LUADB_LMDB_C_FLAGS}"
)
if(WIN32)
    find_library(LIB_KERNEL32 Kernel32)
    if (LIB_KERNEL32_FOUND)
        target_link_libraries(lmdb LIB_KERNEL32)
    endif(LIB_KERNEL32_FOUND)
endif(WIN32)

##############################
# Generate Executable
##############################

add_executable(luadb ${SOURCE_FILES})
set_target_properties(luadb PROPERTIES
    COMPILE_FLAGS "${LUADB_C_FLAGS}"
)

##############################
# Link in Libraries
##############################

target_link_libraries(luadb fastcgi)
target_link_libraries(luadb json)
target_link_libraries(luadb luacore)
target_link_libraries(luadb lual -lm)
target_link_libraries(luadb lmdb)

if (LIB_READLINE)
    target_link_libraries(luadb ${LIB_READLINE})
endif(LIB_READLINE)

##############################
# Install Support Files
##############################

# Install to a different location for Windows
if(NOT WIN32)
    install(DIRECTORY
            DESTINATION /etc/luadb)
    install(DIRECTORY
            DESTINATION /var/www/luadb)
    install(DIRECTORY
            DESTINATION /var/log/luadb)

    install(FILES src/config/config.lua
            DESTINATION /etc/luadb/)
    install(FILES src/web/reqhandler.lua
            DESTINATION /var/www/luadb)
else(NOT WIN32)
    install(DIRECTORY
            DESTINATION "C:/luadb")
    install(DIRECTORY
            DESTINATION "C:/luadb/config")
    install(DIRECTORY
            DESTINATION "C:/luadb/web")
    install(DIRECTORY
            DESTINATION "C:/luadb/log")

    install(FILES src/config/config.lua
            DESTINATION "C:/luadb/config")
    install(FILES src/web/reqhandler.lua
            DESTINATION "C:/luadb/web")
endif(NOT WIN32)