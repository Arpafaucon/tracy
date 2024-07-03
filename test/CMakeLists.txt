cmake_minimum_required(VERSION 3.16)

option(NO_ISA_EXTENSIONS "Disable ISA extensions (don't pass -march=native or -mcpu=native to the compiler)" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/version.cmake)

set(CMAKE_CXX_STANDARD 20)

project(
    tracy-test
    LANGUAGES C CXX ASM
    VERSION ${TRACY_VERSION_STRING}
)

# a bit weird but works:
# include the client cmake config coming from top-level
# cmake needs us to specify the build subfolder -> client/
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. client/)

add_executable(tracy-test test.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC TracyClient)

# OS-specific options
exec_program(uname ARGS -s OUTPUT_VARIABLE SYSTEM_KERNEL)
exec_program(uname ARGS -o OUTPUT_VARIABLE SYSTEM_OS)

if (SYSTEM_KERNEL strequal "Linux")
    target_compile_definitions(tracy-test PRIVATE TRACY_DEBUGINFOD)
    target_link_libraries(tracy-test "debuginfod")
endif()

if (SYSTEM_OS strequal "FreeBSD")
    target_link_libraries(tracy-test "execinfo")
endif()
