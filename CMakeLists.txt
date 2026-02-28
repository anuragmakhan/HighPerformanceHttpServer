cmake_minimum_required(VERSION 3.10)
project(HighPerformanceHttpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Treat warnings as errors for good practice
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

include_directories(include)

add_executable(http_server
    src/main.cpp
    src/server.cpp
)

# Optional threading, though needed later
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(http_server PRIVATE Threads::Threads)
