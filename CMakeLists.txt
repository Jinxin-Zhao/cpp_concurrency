cmake_minimum_required(VERSION 3.20)
project(concurrency)

set(CMAKE_CXX_STANDARD 23)

add_executable(concurrency main.cpp)

target_link_libraries(concurrency pthread rt)
