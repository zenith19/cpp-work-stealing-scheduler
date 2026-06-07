cmake_minimum_required(VERSION 3.20)
project(task_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# The scheduler as a reusable library.
add_library(task_scheduler src/task_scheduler.cpp)
target_include_directories(task_scheduler PUBLIC include)
target_link_libraries(task_scheduler PUBLIC Threads::Threads)

# A small program to exercise the scheduler by hand.
add_executable(demo src/demo.cpp)
target_link_libraries(demo PRIVATE task_scheduler)