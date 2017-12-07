cmake_minimum_required(VERSION 3.8)

# Set project
project(CheckMerge-LLVM VERSION 0.1.0 LANGUAGES CXX)

# Find LLVM cmake config
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Add LLVM build instructions and header files
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Include LLVM specific cmake functions
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Include actual libraries
add_subdirectory(checkmerge)
