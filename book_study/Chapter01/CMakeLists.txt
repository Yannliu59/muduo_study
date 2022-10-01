cmake_minimum_required(VERSION 3.22)
project(muduo_study)

#指定为c14版本
set(CMAKE_CXX_STANDARD 14)
set(SRC_LIST test01.cpp)
include_directories("/usr/boost_1_79_0")

add_executable(test01 ${SER_LIST})

