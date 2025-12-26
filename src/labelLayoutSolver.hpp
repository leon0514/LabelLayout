#ifndef LABEL_LAYOUT_SOLVER_HPP
#define LABEL_LAYOUT_SOLVER_HPP

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <cstring>
#include <cstdint>
#include <random> 


struct LayoutBox {
    float left, top, right, bottom;

    inline float width() const { return right - left; }
    inline float height() const { return bottom - top; }
    inline float area() const { return std::max(0.0f, width()) * std::max(0.0f, height()); }

    static inline float intersectArea(const LayoutBox& box1, const LayoutBox& box2) {
        float l = std::max(box1.left, box2.left);
        float r = std::min(box1.right, box2.right);
        float t = std::max(box1.top, box2.top);
        float b = std::min(box1.bottom, box2.bottom);
        return std::max(0.0f, r - l) * std::max(0.0f, b - t);
    }

    static inline bool intersects(const LayoutBox& box1, const LayoutBox& box2) {
        return (box1.left < box2.right && box1.right > box2.left &&
                box1.top < box2.bottom && box1.bottom > box2.top);
    }
};

struct TextSize {
    int width, height, baseline;
};

struct LayoutResult {
    float x, y;
    int fontSize;
    int width;
    int height;
    int textAscent;
};

struct LayoutConfig {
    int gridSize = 100;
    int spatialIndexThreshold = 20;
    int maxIterations = 20;
    int paddingX = 2;
    int paddingY = 2;

    // 几何偏好权重
    float costTlOuter = 0.0f;
    float costTlInner = 50.0f;
    float costBlOuter = 10.0f;
    float costBlInner = 60.0f;
    float costTrOuter = 20.0f;
    float costTrInner = 70.0f;
    float costBrOuter = 30.0f;
    float costBrInner = 80.0f;
    float costSide    = 40.0f;

    float costSlidingPenalty = 5.0f;
    float costScaleTier      = 10000.0f; // 降低字体缩小的惩罚，鼓励在密集时缩小字体
    
    // 遮挡权重
    float costOccludeObj     = 100000.0f;  
    
    // 标签互斥权重
    // 我们将其视为每像素的惩罚，或者重叠比例的惩罚。
    // 这里定义一个基数，计算时会乘以 (重叠面积 / 自身面积)。
    float costOverlapBase    = 100000.0f; 
    float costSelfOverlap    = 200.0f;
};

class FlatUniformGrid {
public:
    int rows = 0, cols = 0;
    float cellW = 100.0f, cellH = 100.0f;
    
    std::vector<int> gridHead;
    struct Node { int id; int next; };
    std::vector<Node> nodes; 

    FlatUniformGrid() { nodes.reserve(4096); }

    void resize(int w, int h, int gridSize) {
        if (gridSize <= 0) gridSize = 100;
        int newCols = (w + gridSize - 1) / gridSize;
        int newRows = (h + gridSize - 1) / gridSize;

        cellW = (float)gridSize;
        cellH = (float)gridSize;

        if (newCols * newRows > (int)gridHead.size()) {
            gridHead.resize(newCols * newRows, -1);
        }
        cols = newCols;
        rows = newRows;
    }

    void clear() {
        if (!gridHead.empty()) {
            std::fill(gridHead.begin(), gridHead.begin() + (rows * cols), -1);
        }
        nodes.clear();
    }

    inline void insert(int id, const LayoutBox& box) {
        // 边界检查，防止越界崩溃
        int c1 = std::max(0, std::min(cols - 1, (int)(box.left / cellW)));
        int r1 = std::max(0, std::min(rows - 1, (int)(box.top / cellH)));
        int c2 = std::max(0, std::min(cols - 1, (int)(box.right / cellW)));
        int r2 = std::max(0, std::min(rows - 1, (int)(box.bottom / cellH)));

        for (int r = r1; r <= r2; ++r) {
            int rowOffset = r * cols;
            for (int c = c1; c <= c2; ++c) {
                int idx = rowOffset + c;
                nodes.push_back({id, gridHead[idx]});
                gridHead[idx] = (int)nodes.size() - 1;
            }
        }
    }

    template <typename Visitor>
    inline void query(const LayoutBox& box, std::vector<int>& visitedToken, int cookie, Visitor&& visitor) {
        int c1 = std::max(0, std::min(cols - 1, (int)(box.left / cellW)));
        int r1 = std::max(0, std::min(rows - 1, (int)(box.top / cellH)));
        int c2 = std::max(0, std::min(cols - 1, (int)(box.right / cellW)));
        int r2 = std::max(0, std::min(rows - 1, (int)(box.bottom / cellH)));

        for (int r = r1; r <= r2; ++r) {
            int rowOffset = r * cols;
            for (int c = c1; c <= c2; ++c) {
                int nodeIdx = gridHead[rowOffset + c];
                while (nodeIdx != -1) {
                    const auto& node = nodes[nodeIdx];
                    if (visitedToken[node.id] != cookie) {
                        visitedToken[node.id] = cookie;
                        visitor(node.id);
                    }
                    nodeIdx = node.next;
                }
            }
        }
    }
};

// ==========================================
// 标签布局求解器
// ==========================================

class LabelLayoutSolver {
public:
    struct Candidate {
        LayoutBox box;
        float geometricCost; 
        float staticCost;    
        float area;          // 预计算面积
        float invArea;       // 预计算面积倒数
        int16_t fontSize;
        int16_t textAscent;
    };

private:
    struct LayoutItem {
        int id;
        LayoutBox objectBox; 
        uint32_t candStart; 
        uint16_t candCount; 
        int selectedRelIndex; 
        
        LayoutBox currentBox;
        float currentArea; // 缓存当前选中框的面积
        float currentTotalCost;
    };

    LayoutConfig config;
    int canvasWidth;
    int canvasHeight;
    std::function<TextSize(const std::string&, int)> measureFunc;

    std::vector<LayoutItem> items;
    std::vector<Candidate> candidatePool;
    std::vector<int> processOrder; //用于随机化迭代顺序

    FlatUniformGrid grid;
    std::vector<int> visitedCookie;
    int currentCookie = 0;
    std::mt19937 rng; // 随机数生成器

public:
    template <typename Func>
    LabelLayoutSolver(int w, int h, Func&& func, const LayoutConfig& cfg = LayoutConfig())
        : config(cfg), canvasWidth(w), canvasHeight(h), measureFunc(std::forward<Func>(func)), rng(12345) // 固定种子保证结果一致性
    {
        items.reserve(128);
        candidatePool.reserve(4096); 
        visitedCookie.reserve(128);
    }

    void setConfig(const LayoutConfig& cfg) { config = cfg; }
    void setCanvasSize(int w, int h) { canvasWidth = w; canvasHeight = h; }

    void clear() {
        items.clear();
        candidatePool.clear();
        processOrder.clear();
    }

    void add(float l, float t, float r, float b, const std::string& text, int baseFontSize) {
        float w = r - l;
        float h = b - t;
        if (w < 2.0f) { float cx = (l+r)*0.5f; l = cx - 1; r = cx + 1; }
        if (h < 2.0f) { float cy = (t+b)*0.5f; t = cy - 1; b = cy + 1; }

        LayoutItem item;
        item.id = (int)items.size();
        item.objectBox = {std::floor(l), std::floor(t), std::ceil(r), std::ceil(b)};
        item.candStart = (uint32_t)candidatePool.size();
        
        generateCandidatesInternal(item, text, baseFontSize);
        
        item.candCount = (uint16_t)(candidatePool.size() - item.candStart);

        if (item.candCount > 0) {
            item.selectedRelIndex = 0;
            const auto& c = candidatePool[item.candStart];
            item.currentBox = c.box;
            item.currentArea = c.area;
            item.currentTotalCost = c.geometricCost;
        } else {
            // Dummy candidate
            Candidate dummy;
            dummy.box = {0,0,0,0};
            dummy.geometricCost = 1e9f;
            dummy.staticCost = 0;
            dummy.area = 0.1f;
            dummy.invArea = 10.0f;
            dummy.fontSize = (int16_t)baseFontSize;
            dummy.textAscent = 0;
            candidatePool.push_back(dummy);
            
            item.candCount = 1;
            item.selectedRelIndex = 0;
            item.currentBox = dummy.box;
            item.currentArea = 0.1f;
            item.currentTotalCost = 1e9f;
        }

        items.push_back(std::move(item));
    }

    void solve() {
        if (items.empty()) return;
        size_t N = items.size();

        if (visitedCookie.size() < N) visitedCookie.resize(N, 0);
        bool useGrid = (N >= (size_t)config.spatialIndexThreshold);

        if (useGrid) grid.resize(canvasWidth, canvasHeight, config.gridSize);

        if (useGrid) {
            grid.clear();
            for (const auto& item : items) grid.insert(item.id, item.objectBox);
        }

        for (auto& item : items) {
            float minCost = std::numeric_limits<float>::max();
            int bestIdx = 0;

            for (uint32_t i = 0; i < item.candCount; ++i) {
                Candidate& cand = candidatePool[item.candStart + i];
                
                float penalty = 0.0f;

                auto checkStaticConflict = [&](int otherId) {
                    const auto& other = items[otherId];
                    float inter = LayoutBox::intersectArea(cand.box, other.objectBox);
                    if (inter > 0.0f) {

                        float ratio = inter * cand.invArea;
                        if (item.id != other.id) 
                            penalty += ratio * config.costOccludeObj;
                        else 
                            penalty += ratio * config.costSelfOverlap;
                    }
                };

                if (useGrid) {
                    currentCookie++;
                    grid.query(cand.box, visitedCookie, currentCookie, checkStaticConflict);
                } else {
                    for (const auto& other : items) checkStaticConflict(other.id);
                }
                cand.staticCost = penalty;
                
                // 贪心初始化：只看 几何+静态 成本
                float total = cand.geometricCost + cand.staticCost;
                if (total < minCost) {
                    minCost = total;
                    bestIdx = (int)i;
                }
            }
            item.selectedRelIndex = bestIdx;
            const auto& bestCand = candidatePool[item.candStart + bestIdx];
            item.currentBox = bestCand.box;
            item.currentArea = bestCand.area;
            item.currentTotalCost = minCost;
        }

        processOrder.resize(N);
        for(size_t i=0; i<N; ++i) processOrder[i] = (int)i;

        for (int iter = 0; iter < config.maxIterations; ++iter) {
            // 每一轮打乱顺序，避免 "先入为主" 导致的死锁
            // 这种类似 "随机重启" 或 "模拟退火" 的机制对密集布局非常有效
            std::shuffle(processOrder.begin(), processOrder.end(), rng);
            
            int changeCount = 0;

            // 重建 Label Grid
            if (useGrid) {
                grid.clear();
                for (const auto& item : items) grid.insert(item.id, item.currentBox);
            }

            for (int idx : processOrder) {
                auto& item = items[idx];
                
                // 计算当前选中的候选的总 Cost (包含与其他标签的重叠)
                auto calculateDynamicCost = [&](const LayoutBox& box, float invBoxArea, int selfId) -> float {
                    float overlapCost = 0.0f;
                    
                    auto visitor = [&](int otherId) {
                        if (selfId == otherId) return;
                        float inter = LayoutBox::intersectArea(box, items[otherId].currentBox);
                        if (inter > 0.1f) {
                            // 软约束核心：代价与重叠面积成正比
                            // 这样即便必须重叠，也会选择重叠最少的位置
                            float ratio = inter * invBoxArea;
                            overlapCost += ratio * config.costOverlapBase;
                        }
                    };
                    
                    if (useGrid) {
                        currentCookie++; 
                        grid.query(box, visitedCookie, currentCookie, visitor);
                    } else {
                        for (size_t j = 0; j < N; ++j) {
                            if ((int)j != selfId) {
                                float inter = LayoutBox::intersectArea(box, items[j].currentBox);
                                if (inter > 0.1f) {
                                    float ratio = inter * invBoxArea;
                                    overlapCost += ratio * config.costOverlapBase;
                                }
                            }
                        }
                    }
                    return overlapCost;
                };

                // 计算当前的实际成本
                float currentDynamicOverlap = calculateDynamicCost(item.currentBox, candidatePool[item.candStart + item.selectedRelIndex].invArea, item.id);
                float currentRealTotal = candidatePool[item.candStart + item.selectedRelIndex].geometricCost + 
                                         candidatePool[item.candStart + item.selectedRelIndex].staticCost + 
                                         currentDynamicOverlap;

                // 如果当前已经是完美状态 (成本很低)，跳过优化
                // 这里的阈值可以设小一点，例如只容忍微小的几何偏差
                if (currentRealTotal < 1.0f) continue;

                // 寻找更优解
                float bestIterCost = currentRealTotal;
                int bestRelIdx = -1;

                // 遍历所有候选
                for (int i = 0; i < item.candCount; ++i) {
                    if (i == item.selectedRelIndex) continue;

                    const auto& cand = candidatePool[item.candStart + i];

                    // 剪枝：如果 基础成本 已经超过 当前最优总成本，就算没重叠也没戏
                    if (cand.geometricCost + cand.staticCost >= bestIterCost) continue;

                    float newOverlap = calculateDynamicCost(cand.box, cand.invArea, item.id);
                    float newTotal = cand.geometricCost + cand.staticCost + newOverlap;

                    if (newTotal < bestIterCost) {
                        bestIterCost = newTotal;
                        bestRelIdx = i;
                    }
                }

                if (bestRelIdx != -1) {
                    item.selectedRelIndex = bestRelIdx;
                    const auto& newCand = candidatePool[item.candStart + bestRelIdx];
                    item.currentBox = newCand.box;
                    item.currentArea = newCand.area;
                    changeCount++;
                }
            }
            if (changeCount == 0) break;
        }
    }

    std::vector<LayoutResult> getResults() const {
        std::vector<LayoutResult> results;
        results.reserve(items.size());
        for (const auto& item : items) {
            const auto& cand = candidatePool[item.candStart + item.selectedRelIndex];
            results.push_back({
                cand.box.left, cand.box.top, 
                (int)cand.fontSize, 
                (int)cand.box.width(), (int)cand.box.height(),
                (int)cand.textAscent
            });
        }
        return results;
    }

private:
    void generateCandidatesInternal(LayoutItem& item, const std::string& text, int baseFontSize) {
        struct ScaleLevel { float scale; int tier; };
        static const ScaleLevel levels[] = {
            {1.0f, 0}, {0.9f, 1}, {0.8f, 2}, {0.75f, 3} 
        };

        const auto& obj = item.objectBox; 

        for (const auto& lvl : levels) {
            int fontSize = (int)(baseFontSize * lvl.scale);
            if (fontSize < 9) break;

            TextSize ts = measureFunc(text, fontSize);
            
            float fW = std::ceil((float)ts.width + config.paddingX * 2);
            float fH = std::ceil((float)(ts.height + ts.baseline + config.paddingY * 2));
            float scalePenalty = lvl.tier * config.costScaleTier;
            
            float area = fW * fH;
            float invArea = 1.0f / (area > 0.1f ? area : 1.0f);

            auto addCand = [&](float x, float y, float posCost) {
                if (x < 0 || y < 0 || x + fW > canvasWidth || y + fH > canvasHeight) return;
                
                candidatePool.emplace_back();
                auto& c = candidatePool.back();
                c.box = {x, y, x + fW, y + fH};
                c.geometricCost = posCost; 
                c.staticCost = 0; 
                c.area = area;
                c.invArea = invArea;
                c.fontSize = (int16_t)fontSize;
                c.textAscent = (int16_t)ts.height;
            };

            // --- 水平布局 (Top / Bottom) ---
            float minX = obj.left;
            float maxX = std::max(obj.left, obj.right - fW);
            float rangeX = maxX - minX;

            // 在密集场景下，增加采样密度
            int stepsX = (lvl.tier <= 1) ? 8 : 4; 
            if (rangeX < 1.0f) stepsX = 0;
            
            float invStepsX = (stepsX > 0) ? 1.0f / stepsX : 0.0f;

            for (int i = 0; i <= stepsX; ++i) {
                float r = i * invStepsX;
                float x = minX + rangeX * r;
                float dist = std::abs(r - 0.5f) * 2.0f; 
                float posP = dist * config.costSlidingPenalty + scalePenalty;

                addCand(x, obj.top - fH, config.costTlOuter + posP); 
                addCand(x, obj.top,      config.costTlInner + posP); 
                addCand(x, obj.bottom,   config.costBlOuter + posP); 
                addCand(x, obj.bottom - fH, config.costBlInner + posP); 
            }

            // --- 垂直布局 (Left / Right) ---
            float minY = obj.top;
            float maxY = std::max(obj.top, obj.bottom - fH);
            float rangeY = maxY - minY;

            int stepsY = (lvl.tier <= 1) ? 8 : 4;
            if (rangeY < 1.0f) stepsY = 0;
            float invStepsY = (stepsY > 0) ? 1.0f / stepsY : 0.0f;

            for (int i = 0; i <= stepsY; ++i) {
                float r = i * invStepsY;
                float y = minY + rangeY * r;
                float dist = std::abs(r - 0.5f) * 2.0f;
                float posP = config.costSide + dist * config.costSlidingPenalty + scalePenalty;

                addCand(obj.left - fW, y, posP); 
                addCand(obj.right,     y, posP); 
            }
        }
    }
};

#endif