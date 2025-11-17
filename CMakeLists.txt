cmake_minimum_required(VERSION 3.16)
project(Assembler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)
# Source files
add_executable(Assembler
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/instruction_encoding.cpp
)
