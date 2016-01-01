cmake_minimum_required(VERSION 3.3)
project(kaleidoscope)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp lexer.h Ast.cpp Ast.h common.h parser.h.h)
add_executable(kaleidoscope ${SOURCE_FILES})