cmake_minimum_required(VERSION 3.15)
project(frostpack)

find_package(pybind11 REQUIRED)

pybind11_add_module(frostpack bindings.cpp)
target_include_directories(frostpack PRIVATE ..)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(frostpack PRIVATE -static-libstdc++ -static-libgcc)
endif()
