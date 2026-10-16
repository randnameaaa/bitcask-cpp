cmake_minimum_required(VERSION 3.10)
project(BitcaskEngine)

# 开启 C++ 17 标准（现代 C++ 开发标配）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 告诉编译器去哪里找头文件
include_directories(include)

# 把 src/main.cpp 编译成一个叫 bitcask_test 的可执行文件
add_executable(bitcask_test src/main.cpp src/log_record.cpp src/file_io.cpp src/engine.cpp)