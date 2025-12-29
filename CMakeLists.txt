cmake_minimum_required(VERSION 3.15)
project(LabelLayout)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(pybind11 CONFIG REQUIRED)

# 模块定义
pybind11_add_module(labellayout src/interface.cpp)

install(TARGETS labellayout DESTINATION .)