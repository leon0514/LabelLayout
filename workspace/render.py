import labellayout
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import random

# ==========================================
# 1. 基础配置
# ==========================================
def py_measure_func(text: str, font_size: int) -> labellayout.TextSize:
    # 模拟字体测量
    w = int(len(text) * font_size * 0.6)
    h = int(font_size * 1.2) # 稍微增加一点高度容余，让框好看点
    return labellayout.TextSize(w, h, 0)

CANVAS_W, CANVAS_H = 800, 600
cfg = labellayout.LayoutConfig()
cfg.gridSize = 50
cfg.paddingX = 2
cfg.paddingY = 2

solver = labellayout.LabelLayout(CANVAS_W, CANVAS_H, py_measure_func, cfg)

# ==========================================
# 2. 数据生成 & 原始状态计算
# ==========================================
random.seed(42)
objects = []  # (l, t, r, b)
labels = []   # text
naive_placements = [] # 存储左侧“无算法”时的标签位置

# 定义添加物体的辅助函数
def add_object(cx, cy, w, h, text, font_size):
    l, t, r, b = cx - w/2, cy - h/2, cx + w/2, cy + h/2
    objects.append((l, t, r, b))
    labels.append(text)
    
    # 1. 添加到 Solver (右侧使用)
    solver.add(l, t, r, b, text, font_size)
    
    # 2. 计算朴素摆放位置 (左侧 YOLO 风格)
    # 【逻辑修改】：标签位于物体正上方，左对齐
    ts = py_measure_func(text, font_size)
    lbl_w, lbl_h = ts.width, ts.height
    
    lbl_x = l  # 左对齐
    lbl_y = t - lbl_h # 放在上面 (top - height)
    
    # 如果超出了上边界 (比如物体贴着顶边)，一般 YOLO 会把标签画在框内
    # 这里简单模拟：如果 y < 0，就画在框内 (t)，保证不消失，但遵循“尽量在上方”
    if lbl_y < 0:
        lbl_y = t 

    naive_placements.append({
        'x': lbl_x, 'y': lbl_y, 
        'w': lbl_w, 'h': lbl_h, 
        'fs': font_size
    })

# A. 中心拥挤区 (制造垂直堆叠，展示标签遮挡问题)
for i in range(8):
    # 让y轴分布稍微密集一点，容易展示“下面标签挡住上面物体”
    cx, cy = 400 + random.randint(-40, 40), 300 + random.randint(-60, 60)
    add_object(cx, cy, 40, 40, f"Obj-{i}", 20)

# B. 边缘随机区
for i in range(10):
    cx, cy = random.randint(100, 700), random.randint(100, 500)
    if 300 < cx < 500 and 200 < cy < 400: continue
    add_object(cx, cy, random.randint(30, 60), random.randint(30, 60), f"Side-{i}", 24)

# ==========================================
# 3. 双屏动画逻辑
# ==========================================
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# 设置通用属性
for ax in [ax1, ax2]:
    ax.set_xlim(0, CANVAS_W)
    ax.set_ylim(CANVAS_H, 0)
    ax.set_aspect('equal')
    # 绘制静态物体背景（红框）
    for (l, t, r, b) in objects:
        # 红色物体框
        rect = Rectangle((l, t), r-l, b-t, linewidth=2, edgecolor='red', facecolor='none')
        ax.add_patch(rect)
        # 淡淡的填充
        rect_fill = Rectangle((l, t), r-l, b-t, linewidth=0, facecolor='salmon', alpha=0.3)
        ax.add_patch(rect_fill)

ax1.set_title("Standard Visualization (YOLO Style)\nLabels Block Neighboring Objects", color='red', fontweight='bold')

# 容器
artists = []

# 动画参数
FPS = 5
ANIMATION_FRAMES = 25 
PAUSE_SECONDS = 2
PAUSE_FRAMES = PAUSE_SECONDS * FPS
TOTAL_FRAMES = ANIMATION_FRAMES + PAUSE_FRAMES

def update(frame):
    # 清理上一帧的动态元素
    for art in artists: 
        art.remove()
    artists.clear()
    
    # ---------------------------------------
    # 左侧绘制 (ax1): YOLO 风格 (标签在上方)
    # ---------------------------------------
    for i, item in enumerate(naive_placements):
        # 标签背景：模仿您给的图片，蓝色背景
        # 注意：这里完全没有避让逻辑，所以标签会互相重叠，或者挡住上面的物体
        rect = Rectangle((item['x'], item['y']), item['w'], item['h'], 
                         linewidth=0, facecolor='#4da6ff', alpha=0.9) # 亮蓝色
        ax1.add_patch(rect)
        artists.append(rect)
        
        # 边框
        rect_border = Rectangle((item['x'], item['y']), item['w'], item['h'], 
                         linewidth=1, edgecolor='blue', facecolor='none')
        ax1.add_patch(rect_border)
        artists.append(rect_border)
        
        # 文字
        txt = ax1.text(item['x'] + item['w']/2, item['y'] + item['h']/2, labels[i],
                       ha='center', va='center', fontsize=9, color='white', fontweight='bold')
        artists.append(txt)

    # ---------------------------------------
    # 右侧绘制 (ax2): 算法演进
    # ---------------------------------------
    if frame < ANIMATION_FRAMES:
        iters = 0
        if frame > 5: iters = frame - 5
        
        cfg.maxIterations = iters
        solver.set_config(cfg)
        solver.solve()
        title_text = f"Layout Solver (Optimizing...)\nIterations: {iters}"
    else:
        title_text = "Layout Solver (Final Result)\nIntelligent Placement"
    
    ax2.set_title(title_text, color='green', fontweight='bold')
    
    results = solver.layout()
    
    for i, res in enumerate(results):
        obj = objects[i]
        
        # 标签框
        color = '#4da6ff' if res.fontSize >= 18 else 'orange'
        rect = Rectangle((res.left, res.top), res.width, res.height, 
                         linewidth=1, edgecolor='blue', facecolor=color, alpha=0.9)
        ax2.add_patch(rect)
        artists.append(rect)
        
        # 文字
        txt = ax2.text(res.left + res.width/2, res.top + res.height/2, labels[i],
                      ha='center', va='center', fontsize=9, color='white', fontweight='bold')
        artists.append(txt)
        
        # 连线
        obj_cx = (obj[0] + obj[2]) / 2
        obj_cy = (obj[1] + obj[3]) / 2
        lbl_cx = res.left + res.width / 2
        lbl_cy = res.top + res.height / 2
        line, = ax2.plot([obj_cx, lbl_cx], [obj_cy, lbl_cy], color='gray', linestyle=':', linewidth=1)
        artists.append(line)

    return artists

def init():
    return []

print(f"Generating Comparison GIF ({TOTAL_FRAMES} frames)...")
ani = animation.FuncAnimation(fig, update, frames=TOTAL_FRAMES, init_func=init, blit=False)
ani.save('demo.gif', writer='pillow', fps=FPS)
print("Done! Saved to demo.gif")