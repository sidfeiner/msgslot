cmake_minimum_required(VERSION 3.17)
project(msgslot C)

set(CMAKE_C_STANDARD 11)
include_directories(/usr/src/linux-headers-5.0.0-23/include)
add_executable(reader message_reader.c)
add_executable(sender message_sender.c)
add_executable(tester1 tester.c)
add_executable(tester2 tester2.c)