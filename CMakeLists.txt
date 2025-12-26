cmake_minimum_required(VERSION 3.15)
project(LabelLayoutDemo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)


find_package(Python3 COMPONENTS Interpreter Development REQUIRED)


execute_process(
    COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE _pybind11_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _pybind11_status
)

if(_pybind11_status EQUAL 0)
    set(pybind11_DIR "${_pybind11_dir}")
    message(STATUS "Pybind11 found at: ${pybind11_DIR}")
else()
    message(WARNING "Could not find pybind11 via python. Ensure 'pip install pybind11' is run.")
endif()

find_package(pybind11 CONFIG REQUIRED)
find_package(OpenCV QUIET)

pybind11_add_module(layout_solver src/interface.cpp)

set_target_properties(layout_solver PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workspace"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workspace"
)


if(OpenCV_FOUND)
    add_executable(demo_cpp src/main.cpp)
    target_link_libraries(demo_cpp ${OpenCV_LIBS})
    set_target_properties(demo_cpp PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workspace"
    )
endif()