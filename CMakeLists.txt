cmake_minimum_required(VERSION 3.17)
project(msgslot C)

set(CMAKE_C_STANDARD 11)
include_directories(/usr/src/linux-headers-5.0.0-23/include)
add_executable(msgslot main.c)