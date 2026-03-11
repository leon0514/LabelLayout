cmake_minimum_required(VERSION 3.15)
project(labellayout)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pybind11 REQUIRED)

# 创建模块
pybind11_add_module(_labellayout_backend src/interface.cpp)
target_include_directories(_labellayout_backend PRIVATE src)

# 关键修复：确保模块被正确安装到 Python 包目录
# scikit-build-core 自动处理安装路径，这里只需要指定安装目标
install(TARGETS _labellayout_backend
        LIBRARY DESTINATION labellayout) 

# 必须显式安装 Python 包中的 __init__.py 等文件
# 假设你的项目结构中有 labellayout/__init__.py
install(TARGETS _labellayout_backend
        LIBRARY DESTINATION labellayout)