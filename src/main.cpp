#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <random>
#include <iostream>
#include <chrono> // 用于计时
#include "labelLayout.hpp"

// 用于测试的数据结构
struct TestObject {
    float x, y, w, h;
    std::string name;
    int id;
};

int main() {
    // 1. 画布设置
    int width = 1280;
    int height = 720;
    // 这里的背景设为深色，模拟监控或工业场景
    cv::Mat canvas(height, width, CV_8UC3, cv::Scalar(40, 40, 40)); 

    // 2. 准备配置参数 (针对密集场景调优)
    LayoutConfig config;
    config.paddingX = 4;
    config.paddingY = 4;
    config.gridSize = 40;              // 密集场景下，网格越小，空间查询越准
    config.spatialIndexThreshold = 10; // 很早就开启 Grid 索引加速
    config.maxIterations = 40;         // 增加迭代次数，让随机扰动（Shuffle）有足够时间收敛
    
    // 【关键参数调整】
    // 降低重叠的基础惩罚，配合基于面积的软约束，让标签敢于在不得不重叠时“贴贴”
    config.costOverlapBase = 200000.0f; 
    // 提高遮挡物体的惩罚，尽量不要挡住红框
    config.costOccludeObj  = 50000.0f;  
    // 适当降低字体缩小的惩罚，鼓励在拥挤时缩小字体
    config.costScaleTier   = 5000.0f;   

    // 3. 定义文本测量函数 (结合 OpenCV)
    // Hershey Simplex: scale 1.0 ~ 22px height
    auto measureFunc = [](const std::string& text, int fontSize) -> TextSize {
        double fontScale = fontSize / 22.0;
        int thickness = 1;
        int baseline = 0;
        cv::Size s = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
        return {s.width, s.height, baseline};
    };

    // 4. 初始化求解器
    LabelLayout solver(width, height, measureFunc, config);

    // 5. 生成随机测试数据
    std::vector<TestObject> testObjects;
    std::mt19937 rng(100); // 换个种子看不同的布局
    
    // 让物体主要集中在画布中间区域，制造人为的“拥挤”
    std::uniform_int_distribution<> posX(200, width - 200);
    std::uniform_int_distribution<> posY(150, height - 150);
    std::uniform_int_distribution<> sizeDist(20, 80); // 物体都不大，但很密

    int numObjects = 80; // 生成80个物体，非常拥挤

    for (int i = 0; i < numObjects; ++i) {
        TestObject obj;
        obj.x = (float)posX(rng);
        obj.y = (float)posY(rng);
        obj.w = (float)sizeDist(rng);
        obj.h = (float)sizeDist(rng);
        obj.name = "ID:" + std::to_string(i);
        obj.id = i;
        testObjects.push_back(obj);

        // 添加到求解器，基础字号设为 14px
        solver.add(obj.x, obj.y, obj.x + obj.w, obj.y + obj.h, obj.name, 14);
    }

    // 6. 执行布局求解
    auto start = std::chrono::high_resolution_clock::now();
    
    solver.solve();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = end - start;
    std::cout << "Layout solved in: " << ms.count() << " ms for " << numObjects << " items." << std::endl;

    // 7. 获取结果并绘制
    std::vector<LayoutResult> results = solver.layout();

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& res = results[i];
        const auto& obj = testObjects[i];

        // --- A. 绘制原始物体框 (红色) ---
        cv::Rect objRect((int)obj.x, (int)obj.y, (int)obj.w, (int)obj.h);
        cv::rectangle(canvas, objRect, cv::Scalar(50, 50, 200), 2);

        // --- B. 绘制连接线 ---
        cv::Point objCenter(objRect.x + objRect.width / 2, objRect.y + objRect.height / 2);
        cv::Point labelCenter(res.x + res.width / 2, res.y + res.height / 2);
        
        // 线条颜色稍微淡一点
        cv::line(canvas, objCenter, labelCenter, cv::Scalar(150, 150, 150), 1, cv::LINE_AA);

        // --- C. 绘制标签 ---
        cv::Rect labelRect((int)res.x, (int)res.y, res.width, res.height);
        
        // 根据字体大小改变背景颜色，直观显示哪些标签被“压缩”了
        cv::Scalar bgColor;
        if (res.fontSize >= 14) bgColor = cv::Scalar(220, 220, 220); // 正常：白灰
        else if (res.fontSize >= 12) bgColor = cv::Scalar(200, 220, 255); // 轻微压缩：淡黄(OpenCV BGR -> 淡橙)
        else bgColor = cv::Scalar(180, 180, 255); // 严重压缩：深橙/红

        // 填充背景
        cv::rectangle(canvas, labelRect, bgColor, -1);
        
        // 绘制边框 (如果重叠严重，边框能帮助区分)
        cv::rectangle(canvas, labelRect, cv::Scalar(50, 50, 50), 1);

        // --- D. 绘制文字 ---
        double fontScale = res.fontSize / 22.0;
        int textY = (int)res.y + config.paddingY + res.textAscent;

        cv::putText(canvas, obj.name, 
                    cv::Point((int)res.x + config.paddingX, textY),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, 
                    cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
    }

    // 保存结果
    cv::imwrite("dense_result.jpg", canvas);
    std::cout << "Result saved to dense_result.jpg" << std::endl;

    return 0;
}