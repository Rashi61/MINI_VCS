
cmake_minimum_required(VERSION 3.20)

project(MiniVCS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_executable(minivcs
    src/main.cpp
    src/repository/repository.cpp
    src/vcs/FileManager.cpp
    src/security/HashManager.cpp
    src/vcs/commit.cpp
    src/vcs/CommitManager.cpp
)

target_include_directories(minivcs PRIVATE include)
target_link_libraries(minivcs PRIVATE OpenSSL::Crypto)