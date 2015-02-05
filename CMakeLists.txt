cmake_minimum_required(VERSION 2.8.4)
project(dpi)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lsetupapi -mwindows")

set(SOURCE_FILES main.c)
add_executable(dpi ${SOURCE_FILES})

target_link_libraries(dpi libsetupapi.a)