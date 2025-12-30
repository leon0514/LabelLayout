## ultralytics 画图补丁
这是一个用于 Ultralytics YOLO 的补丁工具，用于解决密集目标检测时的标签重叠问题。

## 安装
```shell
pip install smart-label-plotter
```

### 如何使用
```python
from smart_label_plotter import SmartLabelPlotter
from ultralytics import YOLO

# 1. 初始化并注册补丁
plotter = SmartLabelPlotter()
plotter.register()

# 2. 正常运行 YOLO 推理
model = YOLO("yolov8n.pt")
results = model("path/to/image.jpg")

# 3. 绘制结果 (补丁会自动生效)
res_plotted = results[0].plot()
```


### 效果图
![效果图](result.jpg)