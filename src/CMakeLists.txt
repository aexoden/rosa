file(GLOB SOURCE_FILES "*.cc")

add_executable(spoony ${SOURCE_FILES})

target_link_libraries(spoony ${GLIBMM_LIBRARIES} ${GIOMM_LIBRARIES})
