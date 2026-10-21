cmake_minimum_required(VERSION 3.20)

project(MiniVCS LANGUAGES C CXX)

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
    src/RetentionManager.cpp
    src/vcs/DiffManager.cpp
    
       # --- Member 2 files (branch, merge) ---

    # --- Member 3 files (database, auth) ---
    src/member3.cpp
    third_party/sqlite3/sqlite3.c

    # --- Member 4 files (remote) --- 
)

target_include_directories(minivcs PRIVATE
    include
    third_party/sqlite3
)

target_link_libraries(minivcs PRIVATE
    OpenSSL::Crypto
    bcrypt
)


enable_testing()

find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    add_test(
        NAME minivcs_cli_tests
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/run_tests.py
                $<TARGET_FILE:minivcs>
    )
endif()