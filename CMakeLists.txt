cmake_minimum_required(VERSION 3.20)
project(task_scheduler LANGUAGES CXX)

# Enforce C++20, no compiler-specific extensions.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Threads::Threads pulls in pthread, needed by std::thread/std::jthread on Linux.
find_package(Threads REQUIRED)

add_executable(hello src/hello.cpp)
target_link_libraries(hello PRIVATE Threads::Threads)
