cmake_minimum_required(VERSION 3.10)

set(_TARGET_NAME "test_iterator")


alpaka_add_executable(
        ${_TARGET_NAME}
        src/Iterator.cpp)
target_include_directories(
        ${_TARGET_NAME}
        PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(${_TARGET_NAME} PRIVATE ${alpaka_INCLUDE_DIRS})
target_link_libraries(
        ${_TARGET_NAME}
        PRIVATE "vikunjaTestLib")

set_target_properties(${_TARGET_NAME} PROPERTIES FOLDER "test/unit")

add_test(NAME ${_TARGET_NAME} COMMAND ${_TARGET_NAME} ${_VIKUNJA_TEST_OPTIONS})