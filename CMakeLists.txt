cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -march=native -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -pedantic -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded")

project(rosa)

find_package(Boost 1.65)

if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
endif()

find_library(LMDB_LIBRARY NAMES lmdb)

link_libraries(stdc++fs ${LMDB_LIBRARY})

link_directories(${GLIBMM_LIBRARY_DIRS} ${GIOMM_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

include_directories(SYSTEM include)
add_subdirectory(src)
