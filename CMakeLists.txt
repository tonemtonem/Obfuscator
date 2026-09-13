cmake_minimum_required(VERSION 4.2)
project(Obfuscator LANGUAGES CXX)


if(DEFINED ENV{LLVM_DIR} AND NOT LLVM_DIR)
    set(LLVM_DIR "$ENV{LLVM_DIR}" CACHE PATH "Path to LLVMConfig.cmake" FORCE)
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM_INCLUDE_DIRS (from LLVMConfig): ${LLVM_INCLUDE_DIRS}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(MyPass MODULE mypass.cpp)

set(LLVM_EXTRA_INCLUDE_CANDIDATES
        "/usr/include/llvm-${LLVM_VERSION_MAJOR}"
        "/usr/lib/llvm-${LLVM_VERSION_MAJOR}/include"
)

foreach(_dir ${LLVM_EXTRA_INCLUDE_CANDIDATES})
    if(EXISTS "${_dir}/llvm")
        message(STATUS "Adding extra LLVM include dir: ${_dir}")
        list(APPEND LLVM_EXTRA_INCLUDES "${_dir}")
    endif()
endforeach()

target_include_directories(MyPass PRIVATE
        ${LLVM_INCLUDE_DIRS}
        ${LLVM_EXTRA_INCLUDES}
)
target_compile_definitions(MyPass PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(MyPass PROPERTIES
        PREFIX ""
)