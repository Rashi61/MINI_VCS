cmake_minimum_required(VERSION 3.20)

project(MiniVCS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

# Main MiniVCS executable
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
    src/vcs/MergeManager.cpp
    src/RemoteManager.cpp
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

# Branch-management tests
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

# Enable CTest
enable_testing()

# Merge-management integration tests
add_executable(minivcs_merge_tests
    tests/test_merge.cpp
    src/repository/repository.cpp
    src/vcs/MergeManager.cpp
    src/vcs/branch.cpp
    src/vcs/CommitManager.cpp
    src/vcs/commit.cpp
    src/vcs/FileManager.cpp
    src/security/HashManager.cpp
    src/RetentionManager.cpp
    src/vcs/DiffManager.cpp
)

target_include_directories(minivcs_merge_tests PRIVATE
    include
)

target_link_libraries(minivcs_merge_tests PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME minivcs_merge_tests
    COMMAND minivcs_merge_tests
)

# Python regression and integrity tests
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
        NAME minivcs_merge_cli_tests
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/merge_cli_tests.py
                $<TARGET_FILE:minivcs>
    )

    add_test(
        NAME minivcs_member1_integrity
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/member1_integrity.py
                $<TARGET_FILE:minivcs>
    )
endif()

# Branch-management CTest registration
add_test(
    NAME minivcs_branch_tests
    COMMAND minivcs_branch_tests
)

# ---- Member 4: remote repository tests ----

add_executable(minivcs_remote_tests
    tests/remote_test.cpp
    src/RemoteManager.cpp
)

target_include_directories(minivcs_remote_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME minivcs_remote_tests
    COMMAND minivcs_remote_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)