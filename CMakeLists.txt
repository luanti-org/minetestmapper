cmake_minimum_required(VERSION 3.5)

project(minetestmapper
	VERSION 1.0
	LANGUAGES CXX
)

# Stuff & Paths

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
	set(SHAREDIR ".")
	set(BINDIR ".")
	set(DOCDIR ".")
else()
	set(SHAREDIR "share/luanti") # reuse engine share dir
	set(BINDIR "bin")
	set(DOCDIR "share/doc/${PROJECT_NAME}")
	set(MANDIR "share/man")
endif()

set(CUSTOM_SHAREDIR "" CACHE STRING "Directory to install data files into")
if(NOT CUSTOM_SHAREDIR STREQUAL "")
        set(SHAREDIR "${CUSTOM_SHAREDIR}")
        message(STATUS "Using SHAREDIR=${SHAREDIR}")
endif()

set(CUSTOM_BINDIR "" CACHE STRING "Directory to install binaries into")
if(NOT CUSTOM_BINDIR STREQUAL "")
        set(BINDIR "${CUSTOM_BINDIR}")
        message(STATUS "Using BINDIR=${BINDIR}")
endif()

set(CUSTOM_DOCDIR "" CACHE STRING "Directory to install documentation into")
if(NOT CUSTOM_DOCDIR STREQUAL "")
        set(DOCDIR "${CUSTOM_DOCDIR}")
        message(STATUS "Using DOCDIR=${DOCDIR}")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Libraries: gd

find_library(LIBGD_LIBRARY gd)
find_path(LIBGD_INCLUDE_DIR gd.h)
message (STATUS "libgd library: ${LIBGD_LIBRARY}")
message (STATUS "libgd headers: ${LIBGD_INCLUDE_DIR}")
if(NOT LIBGD_LIBRARY OR NOT LIBGD_INCLUDE_DIR)
	message(FATAL_ERROR "libgd not found!")
endif(NOT LIBGD_LIBRARY OR NOT LIBGD_INCLUDE_DIR)

# Libraries: zlib

find_package(zlib-ng QUIET)
if(zlib-ng_FOUND)
	set(ZLIB_INCLUDE_DIR zlib-ng::zlib)
	set(ZLIB_LIBRARY zlib-ng::zlib)
	set(USE_ZLIB_NG TRUE)
	message(STATUS "Found zlib-ng, using it instead of zlib.")
else()
	message(STATUS "zlib-ng not found, falling back to zlib.")
	find_package(ZLIB REQUIRED)
	set(USE_ZLIB_NG FALSE)
endif()

# Libraries: zstd

find_package(Zstd REQUIRED)

# Libraries: sqlite3

find_library(SQLITE3_LIBRARY sqlite3)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
message (STATUS "sqlite3 library: ${SQLITE3_LIBRARY}")
message (STATUS "sqlite3 headers: ${SQLITE3_INCLUDE_DIR}")
if(NOT SQLITE3_LIBRARY OR NOT SQLITE3_INCLUDE_DIR)
	message(FATAL_ERROR "sqlite3 not found!")
endif(NOT SQLITE3_LIBRARY OR NOT SQLITE3_INCLUDE_DIR)

# Libraries: postgresql

option(ENABLE_POSTGRESQL "Enable PostgreSQL backend" TRUE)
set(USE_POSTGRESQL FALSE)

if(ENABLE_POSTGRESQL)
	if(CMAKE_VERSION VERSION_LESS "3.20")
		find_package(PostgreSQL QUIET)
		# Before CMake 3.20 FindPostgreSQL.cmake always looked for server includes
		# but we don't need them, so continue anyway if only those are missing.
		if(PostgreSQL_INCLUDE_DIR AND PostgreSQL_LIBRARY)
			set(PostgreSQL_FOUND TRUE)
			set(PostgreSQL_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIR})
			set(PostgreSQL_LIBRARIES ${PostgreSQL_LIBRARY})
		endif()
	else()
		find_package(PostgreSQL)
	endif()

	if(PostgreSQL_FOUND)
		set(USE_POSTGRESQL TRUE)
		message(STATUS "PostgreSQL backend enabled")
		# This variable is case sensitive, don't try to change it to POSTGRESQL_INCLUDE_DIR
		message(STATUS "PostgreSQL includes: ${PostgreSQL_INCLUDE_DIRS}")
		include_directories(${PostgreSQL_INCLUDE_DIRS})
	else()
		message(STATUS "PostgreSQL not found!")
		set(PostgreSQL_LIBRARIES "")
	endif()
endif(ENABLE_POSTGRESQL)

# Libraries: leveldb

OPTION(ENABLE_LEVELDB "Enable LevelDB backend" TRUE)
set(USE_LEVELDB FALSE)

if(ENABLE_LEVELDB)
	find_library(LEVELDB_LIBRARY leveldb)
	find_path(LEVELDB_INCLUDE_DIR leveldb/db.h)
	message (STATUS "LevelDB library: ${LEVELDB_LIBRARY}")
	message (STATUS "LevelDB headers: ${LEVELDB_INCLUDE_DIR}")
	if(LEVELDB_LIBRARY AND LEVELDB_INCLUDE_DIR)
		set(USE_LEVELDB TRUE)
		message(STATUS "LevelDB backend enabled")
		include_directories(${LEVELDB_INCLUDE_DIR})
	else()
		message(STATUS "LevelDB not found!")
		set(LEVELDB_LIBRARY "")
	endif()
endif(ENABLE_LEVELDB)

# Libraries: redis

OPTION(ENABLE_REDIS "Enable redis backend" TRUE)
set(USE_REDIS FALSE)

if(ENABLE_REDIS)
	find_library(REDIS_LIBRARY hiredis)
	find_path(REDIS_INCLUDE_DIR hiredis/hiredis.h)
	message (STATUS "redis library: ${REDIS_LIBRARY}")
	message (STATUS "redis headers: ${REDIS_INCLUDE_DIR}")
	if(REDIS_LIBRARY AND REDIS_INCLUDE_DIR)
		set(USE_REDIS TRUE)
		message(STATUS "redis backend enabled")
		include_directories(${REDIS_INCLUDE_DIR})
	else()
		message(STATUS "redis not found!")
		set(REDIS_LIBRARY "")
	endif()
endif(ENABLE_REDIS)

# Compiling & Linking

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/src/cmake_config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_config.h"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2")
	set(CMAKE_CXX_FLAGS_DEBUG "-Og -g2")
	add_compile_options(-Wall -pipe)
elseif(MSVC)
	add_compile_options(/GR- /Zl)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DNDEBUG)
endif()

add_executable(minetestmapper)

target_include_directories(minetestmapper PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}"
)

target_sources(minetestmapper PRIVATE
	src/BlockDecoder.cpp
	src/PixelAttributes.cpp
	src/PlayerAttributes.cpp
	src/TileGenerator.cpp
	src/ZlibDecompressor.cpp
	src/ZstdDecompressor.cpp
	src/Image.cpp
	src/mapper.cpp
	src/util.cpp
	src/log.cpp
	src/db-sqlite3.cpp
	$<$<BOOL:${USE_POSTGRESQL}>:src/db-postgresql.cpp>
	$<$<BOOL:${USE_LEVELDB}>:src/db-leveldb.cpp>
	$<$<BOOL:${USE_REDIS}>:src/db-redis.cpp>
)

target_include_directories(minetestmapper PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_BINARY_DIR}"
	${SQLITE3_INCLUDE_DIR}
	${LIBGD_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${ZSTD_INCLUDE_DIR}
)

target_link_libraries(minetestmapper
	${SQLITE3_LIBRARY}
	${PostgreSQL_LIBRARIES}
	${LEVELDB_LIBRARY}
	${REDIS_LIBRARY}
	${LIBGD_LIBRARY}
	${ZLIB_LIBRARY}
	${ZSTD_LIBRARY}
)

# Installing & Packaging

install(TARGETS "${PROJECT_NAME}" DESTINATION "${BINDIR}")
install(FILES "AUTHORS" DESTINATION "${DOCDIR}")
install(FILES "COPYING" DESTINATION "${DOCDIR}")
install(FILES "README.rst" DESTINATION "${DOCDIR}")
install(FILES "colors.txt" DESTINATION "${SHAREDIR}")
if(UNIX)
	install(FILES "minetestmapper.6" DESTINATION "${MANDIR}/man6")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Overview mapper for Luanti")
set(CPACK_PACKAGE_VENDOR "celeron55")
set(CPACK_PACKAGE_CONTACT "Perttu Ahola <celeron55@gmail.com>")

if(WIN32)
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win32")
	set(CPACK_GENERATOR ZIP)
else()
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-linux")
	set(CPACK_GENERATOR TGZ)
	set(CPACK_SOURCE_GENERATOR TGZ)
endif()

include(CPack)
