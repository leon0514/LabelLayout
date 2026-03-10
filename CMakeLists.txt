cmake_minimum_required(VERSION 3.15)
project(labellayout)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# scikit-build-core 会自动把 pybind11 路径加入 CMAKE_PREFIX_PATH
find_package(pybind11 REQUIRED)

# 创建模块
pybind11_add_module(_labellayout_backend src/interface.cpp)
target_include_directories(_labellayout_backend PRIVATE src)

# 设置输出名称，确保导入时名字一致
set_target_properties(_labellayout_backend PROPERTIES
    PREFIX ""
    OUTPUT_NAME "_labellayout_backend"
)

# 安装到 Python 包目录下
# scikit-build-core 期待 target 被安装到当前目录或子目录
install(TARGETS _labellayout_backend DESTINATION labellayout)