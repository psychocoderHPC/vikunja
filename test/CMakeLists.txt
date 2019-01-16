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
target_include_directories(CatchMain SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(CatchMain SYSTEM PUBLIC ${alpaka_INCLUDE_DIR})
target_include_directories(CatchMain SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(CatchMain SYSTEM PUBLIC ${alpaka_INCLUDE_DIR}/../test/common/include/)
target_link_libraries(CatchMain PUBLIC "alpaka")

# NVCC does not incorporate the COMPILE_OPTIONS of a target but only the CMAKE_CXX_FLAGS
if(ALPAKA_ACC_GPU_CUDA_ENABLE AND NOT ALPAKA_CUDA_COMPILER MATCHES "clang")
    get_target_property(_COMMON_COMPILE_OPTIONS common COMPILE_OPTIONS)
    string(REPLACE ";" " " _COMMON_COMPILE_OPTIONS_STRING "${_COMMON_COMPILE_OPTIONS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_COMMON_COMPILE_OPTIONS_STRING}")
endif()

list(APPEND _ALPAKA_TEST_OPTIONS "--use-colour yes")
add_subdirectory("unit/")