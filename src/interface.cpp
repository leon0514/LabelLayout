#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "labelLayoutSolver.hpp"

namespace py = pybind11;

// 增强 repr 输出，增加 fontSize 信息，因为现在它是动态变化的
std::string result_repr(const LayoutResult& r) {
    return "<LayoutResult x=" + std::to_string(r.x) + 
           " y=" + std::to_string(r.y) + 
           " w=" + std::to_string(r.width) + 
           " h=" + std::to_string(r.height) + 
           " fs=" + std::to_string(r.fontSize) + ">";
}

PYBIND11_MODULE(layout_solver, m) {
    m.doc() = "Pybind11 binding for Optimized LabelLayoutSolver";

    // 1. 绑定基础结构 TextSize
    py::class_<TextSize>(m, "TextSize")
        .def(py::init<int, int, int>(), py::arg("width"), py::arg("height"), py::arg("baseline")=0)
        .def_readwrite("width", &TextSize::width)
        .def_readwrite("height", &TextSize::height)
        .def_readwrite("baseline", &TextSize::baseline);

    // 2. 绑定配置 LayoutConfig
    // 注意：更新了参数名称以匹配新的头文件
    py::class_<LayoutConfig>(m, "LayoutConfig")
        .def(py::init<>())
        // 基础设置
        .def_readwrite("gridSize", &LayoutConfig::gridSize)
        .def_readwrite("spatialIndexThreshold", &LayoutConfig::spatialIndexThreshold)
        .def_readwrite("maxIterations", &LayoutConfig::maxIterations)
        .def_readwrite("paddingX", &LayoutConfig::paddingX)
        .def_readwrite("paddingY", &LayoutConfig::paddingY)
        
        // 核心权重 (Updated)
        .def_readwrite("costOverlapBase", &LayoutConfig::costOverlapBase)
        .def_readwrite("costOccludeObj", &LayoutConfig::costOccludeObj)
        
        // 偏好权重
        .def_readwrite("costScaleTier", &LayoutConfig::costScaleTier)     // 字体缩小惩罚
        .def_readwrite("costSlidingPenalty", &LayoutConfig::costSlidingPenalty);

    // 3. 绑定结果 LayoutResult
    py::class_<LayoutResult>(m, "LayoutResult")
        .def_readonly("x", &LayoutResult::x)
        .def_readonly("y", &LayoutResult::y)
        .def_readonly("width", &LayoutResult::width)
        .def_readonly("height", &LayoutResult::height)
        .def_readonly("fontSize", &LayoutResult::fontSize)
        .def_readonly("textAscent", &LayoutResult::textAscent) // 暴露 ascent 方便绘图
        .def("__repr__", &result_repr);

    // 4. 绑定主类 LabelLayoutSolver
    py::class_<LabelLayoutSolver>(m, "LabelLayoutSolver")
        // 注意：构造函数签名保持不变，但为了灵活性，measure_func 仍然通过 std::function 传递
        // Pybind11 会自动将 Python callable 转换为 C++ std::function
        .def(py::init<int, int, std::function<TextSize(const std::string&, int)>, const LayoutConfig&>(),
             py::arg("w"), py::arg("h"), py::arg("measure_func"), py::arg("config") = LayoutConfig())
        
        .def("set_config", &LabelLayoutSolver::setConfig)
        .def("set_canvas_size", &LabelLayoutSolver::setCanvasSize) // 暴露 setCanvasSize
        .def("clear", &LabelLayoutSolver::clear)
        
        .def("add", &LabelLayoutSolver::add, 
             py::arg("l"), py::arg("t"), py::arg("r"), py::arg("b"), 
             py::arg("text"), py::arg("baseFontSize"))
        
        .def("solve", &LabelLayoutSolver::solve)
        .def("get_results", &LabelLayoutSolver::getResults);
}