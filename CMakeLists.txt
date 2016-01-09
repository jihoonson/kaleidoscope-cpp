cmake_minimum_required(VERSION 3.3)
project(kaleidoscope)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

#add_definitions(-std=c++11 -fPIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES KaleidoscopeJIT.h toy.cpp toy2.cpp toy2.cpp)
add_executable(kaleidoscope ${SOURCE_FILES})

llvm_map_components_to_libnames(llvm_libs core)

target_link_libraries(kaleidoscope ${llvm_libs})