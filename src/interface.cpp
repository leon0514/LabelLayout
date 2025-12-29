#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "labelLayout.hpp"

namespace py = pybind11;

// 增强 repr 输出
std::string result_repr(const LayoutResult& r) {
    return "<LayoutResult left=" + std::to_string(r.left) + 
           " top=" + std::to_string(r.top) + 
           " width=" + std::to_string(r.width) + 
           " height=" + std::to_string(r.height) + 
           " padding_x=" + std::to_string(r.padding_x) + 
           " padding_y=" + std::to_string(r.padding_y) + 
           " textAscent=" + std::to_string(r.textAscent) + 
           " textDescent=" + std::to_string(r.textDescent) +
           " fontSize=" + std::to_string(r.fontSize) + ">";
}

PYBIND11_MODULE(labellayout, m) {
    m.doc() = "Pybind11 binding for Optimized LabelLayout with 4-Anchor Priority";

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
        .def_readonly("left", &LayoutResult::left)
        .def_readonly("top", &LayoutResult::top)
        .def_readonly("width", &LayoutResult::width)
        .def_readonly("height", &LayoutResult::height)
        .def_readonly("fontSize", &LayoutResult::fontSize)
        .def_readonly("padding_x", &LayoutResult::padding_x)
        .def_readonly("padding_y", &LayoutResult::padding_y)
        .def_readonly("textAscent", &LayoutResult::textAscent)
        .def_readonly("textDescent", &LayoutResult::textDescent)
        .def("__repr__", &result_repr);

    py::class_<LabelLayout>(m, "LabelLayout")
        .def(py::init<int, int, std::function<TextSize(const std::string&, int)>, const LayoutConfig&>(),
             py::arg("w"), py::arg("h"), py::arg("measure_func"), py::arg("config") = LayoutConfig())
        
        .def("set_config", &LabelLayout::setConfig)
        .def("set_canvas_size", &LabelLayout::setCanvasSize)
        .def("clear", &LabelLayout::clear)
        .def("add", &LabelLayout::add, 
             py::arg("l"), py::arg("t"), py::arg("r"), py::arg("b"), 
             py::arg("text"), py::arg("baseFontSize"))
        .def("solve", &LabelLayout::solve)
        .def("layout", &LabelLayout::layout);
}