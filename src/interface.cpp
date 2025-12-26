#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "labelLayoutSolver.hpp"

namespace py = pybind11;

// 增强 repr 输出
std::string result_repr(const LayoutResult& r) {
    return "<LayoutResult x=" + std::to_string(r.x) + 
           " y=" + std::to_string(r.y) + 
           " w=" + std::to_string(r.width) + 
           " h=" + std::to_string(r.height) + 
           " fs=" + std::to_string(r.fontSize) + ">";
}

PYBIND11_MODULE(layout_solver, m) {
    m.doc() = "Pybind11 binding for Optimized LabelLayoutSolver with 4-Anchor Priority";

    py::class_<TextSize>(m, "TextSize")
        .def(py::init<int, int, int>(), py::arg("width"), py::arg("height"), py::arg("baseline")=0)
        .def_readwrite("width", &TextSize::width)
        .def_readwrite("height", &TextSize::height)
        .def_readwrite("baseline", &TextSize::baseline);

    py::class_<LayoutConfig>(m, "LayoutConfig")
        .def(py::init<>())
        // 基础设置
        .def_readwrite("gridSize", &LayoutConfig::gridSize)
        .def_readwrite("spatialIndexThreshold", &LayoutConfig::spatialIndexThreshold)
        .def_readwrite("maxIterations", &LayoutConfig::maxIterations)
        .def_readwrite("paddingX", &LayoutConfig::paddingX)
        .def_readwrite("paddingY", &LayoutConfig::paddingY)
        
        // --- 新的核心锚点权重 (匹配 1->2->3->4 逻辑) ---
        .def_readwrite("costPos1_Top", &LayoutConfig::costPos1_Top)
        .def_readwrite("costPos2_Right", &LayoutConfig::costPos2_Right)
        .def_readwrite("costPos3_Bottom", &LayoutConfig::costPos3_Bottom)
        .def_readwrite("costPos4_Left", &LayoutConfig::costPos4_Left)
        
        // 惩罚项
        .def_readwrite("costSlidingPenalty", &LayoutConfig::costSlidingPenalty) // 开启滑动的基准惩罚
        .def_readwrite("costScaleTier", &LayoutConfig::costScaleTier)           // 缩放字号的惩罚
        .def_readwrite("costOccludeObj", &LayoutConfig::costOccludeObj)         // 遮挡物体的惩罚
        .def_readwrite("costOverlapBase", &LayoutConfig::costOverlapBase);      // 标签间重叠的惩罚

    py::class_<LayoutResult>(m, "LayoutResult")
        .def_readonly("x", &LayoutResult::x)
        .def_readonly("y", &LayoutResult::y)
        .def_readonly("width", &LayoutResult::width)
        .def_readonly("height", &LayoutResult::height)
        .def_readonly("fontSize", &LayoutResult::fontSize)
        .def_readonly("textAscent", &LayoutResult::textAscent)
        .def("__repr__", &result_repr);

    py::class_<LabelLayoutSolver>(m, "LabelLayoutSolver")
        .def(py::init<int, int, std::function<TextSize(const std::string&, int)>, const LayoutConfig&>(),
             py::arg("w"), py::arg("h"), py::arg("measure_func"), py::arg("config") = LayoutConfig())
        
        .def("set_config", &LabelLayoutSolver::setConfig)
        .def("set_canvas_size", &LabelLayoutSolver::setCanvasSize)
        .def("clear", &LabelLayoutSolver::clear)
        .def("add", &LabelLayoutSolver::add, 
             py::arg("l"), py::arg("t"), py::arg("r"), py::arg("b"), 
             py::arg("text"), py::arg("baseFontSize"))
        .def("solve", &LabelLayoutSolver::solve)
        .def("get_results", &LabelLayoutSolver::getResults);
}