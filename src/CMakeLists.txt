file(GLOB SOURCE_FILES "*.cc")

add_executable(rosa ${SOURCE_FILES})

target_link_libraries(rosa)
