cmake_minimum_required(VERSION 3.20)
project(task_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(task_scheduler src/task_scheduler.cpp)
target_include_directories(task_scheduler PUBLIC include)
target_link_libraries(task_scheduler PUBLIC Threads::Threads)

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