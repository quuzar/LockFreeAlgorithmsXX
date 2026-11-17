cmake_minimum_required(VERSION 3.28)
project(LockFreeAlg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(LockFreeAlg
        src/main.cpp
)

target_include_directories(LockFreeAlg PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_include_directories(LockFreeAlg PRIVATE
        "${PROJECT_SOURCE_DIR}/tests"
)
