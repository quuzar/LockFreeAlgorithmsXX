cmake_minimum_required(VERSION 3.28)
project(LockFreeAlg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(LockFreeAlg
        src/main.cpp
)

