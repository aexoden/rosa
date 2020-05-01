file(GLOB SOURCE_FILES "*.cc")

add_executable(rosa ${SOURCE_FILES})

target_include_directories(rosa PUBLIC ../include)

target_link_libraries(rosa PRIVATE stdc++fs ${LMDB_LIBRARY})

target_compile_features(rosa PUBLIC cxx_std_17)

target_compile_options(rosa PRIVATE -O2 -march=native -Wall -pedantic)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(rosa PRIVATE -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded)
endif()
