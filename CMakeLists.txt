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
    src/vcs/IgnoreMatcher.cpp
    src/security/HashManager.cpp
    src/security/PasswordHasher.cpp
    src/vcs/commit.cpp
    src/vcs/CommitManager.cpp
    src/vcs/IntegrityManager.cpp
    src/RetentionManager.cpp
    src/vcs/DiffManager.cpp
    src/vcs/branch.cpp
    src/member3.cpp
    third_party/sqlite3/sqlite3.c
)

target_include_directories(minivcs PRIVATE
    include
    third_party/sqlite3
)

target_link_libraries(minivcs PRIVATE
    OpenSSL::Crypto
)
add_executable(minivcs_branch_tests
    tests/test_branching.cpp
    src/vcs/branch.cpp
    src/vcs/CommitManager.cpp
    src/vcs/commit.cpp
    src/vcs/FileManager.cpp
    src/security/HashManager.cpp
    src/RetentionManager.cpp
    src/vcs/DiffManager.cpp
)

target_include_directories(minivcs_branch_tests PRIVATE
    include
)

target_link_libraries(minivcs_branch_tests PRIVATE
    OpenSSL::Crypto
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

    add_test(
        NAME minivcs_member1_regressions
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/member1_regressions.py
                $<TARGET_FILE:minivcs>
    )

    add_test(
        NAME minivcs_member1_integrity
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/member1_integrity.py
                $<TARGET_FILE:minivcs>
    )
endif()

add_test(
    NAME minivcs_branch_tests
    COMMAND minivcs_branch_tests
)


