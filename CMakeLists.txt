
cmake_minimum_required(VERSION 3.20)

project(MiniVCS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minivcs
    src/main.cpp
    src/repository/Repository.cpp
    src/vcs/FileManager.cpp
)

target_include_directories(minivcs PRIVATE include)
