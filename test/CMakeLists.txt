cmake_minimum_required(VERSION 3.15)
project(AIAnalyzer)

set(CMAKE_CXX_STANDARD 17)

# 1. 查找系统的线程库
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(AIAnalyzer main.cpp)

# 2. 将线程库链接到你的可执行文件中
target_link_libraries(AIAnalyzer PRIVATE Threads::Threads)