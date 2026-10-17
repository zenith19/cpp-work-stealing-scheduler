cmake_minimum_required(VERSION 3.20)
project(task_scheduler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Demos, benchmark and tests are for people working on the scheduler. A project
# consuming it through add_subdirectory() or FetchContent gets the library only.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(TASK_SCHEDULER_IS_TOP_LEVEL ON)
else()
    set(TASK_SCHEDULER_IS_TOP_LEVEL OFF)
endif()
option(TASK_SCHEDULER_BUILD_EXTRAS
       "Build the demos, benchmark and unit tests"
       ${TASK_SCHEDULER_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)

add_library(task_scheduler src/task_scheduler.cpp)
add_library(ts::task_scheduler ALIAS task_scheduler)

target_include_directories(task_scheduler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(task_scheduler PUBLIC cxx_std_20)
target_link_libraries(task_scheduler PUBLIC Threads::Threads)

# Warnings are on for our own code but never fatal locally, and PRIVATE so they
# don't leak into a consumer's build. CI is where -Werror belongs.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(task_scheduler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- install / export -------------------------------------------------------

install(TARGETS task_scheduler EXPORT task_schedulerTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT task_schedulerTargets
        FILE task_schedulerTargets.cmake
        NAMESPACE ts::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task_scheduler)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/task_schedulerConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    cmake/task_schedulerConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/task_schedulerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task_scheduler)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/task_schedulerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/task_schedulerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task_scheduler)

# --- demos, benchmark, tests ------------------------------------------------

if(NOT TASK_SCHEDULER_BUILD_EXTRAS)
    return()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(demo src/demo.cpp)
target_link_libraries(demo PRIVATE task_scheduler)

add_executable(visual_demo src/visual_demo.cpp)
target_link_libraries(visual_demo PRIVATE task_scheduler)

add_executable(benchmark benchmarks/benchmark.cpp)
target_link_libraries(benchmark PRIVATE task_scheduler)

enable_testing()
add_executable(unit_tests tests/test_task_scheduler.cpp)
target_link_libraries(unit_tests PRIVATE task_scheduler)
add_test(NAME unit_tests COMMAND unit_tests)
