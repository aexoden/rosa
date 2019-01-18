link_directories(${GLIBMM_LIBRARY_DIRS} ${GIOMM_LIBRARY_DIRS})
include_directories(${GLIBMM_INCLUDE_DIRS} ${GIOMM_INCLUDE_DIRS})

file(GLOB SOURCE_FILES "*.cc")

add_executable(spoony ${SOURCE_FILES})

target_link_libraries(spoony ${GLIBMM_LIBRARIES} ${GIOMM_LIBRARIES})
