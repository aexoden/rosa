cmake_minimum_required(VERSION 3.10...3.17)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(rosa LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.65 REQUIRED)

find_library(LMDB_LIBRARY NAMES lmdb)

add_subdirectory(src)
