cmake_minimum_required(VERSION 3.10)

add_library(CatchMain CatchMain.cpp)

set_target_properties(CatchMain PROPERTIES
        FOLDER "test"
        CXX_STANDARD 11
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(CatchMain PUBLIC "CATCH_CONFIG_FAST_COMPILE")
target_include_directories(CatchMain SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../thirdParty/catch2/include)

# Link other tests against this library to avoid repeated compilation of Catch2.
add_library(vikunjaTestLib dummy.cpp)
target_include_directories(vikunjaTestLib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vikunjaTestLib PUBLIC vikunja::internalvikunja CatchMain)

list(APPEND _VIKUNJA_TEST_OPTIONS "--use-colour yes")
add_subdirectory("unit/")
add_subdirectory("integ/")
