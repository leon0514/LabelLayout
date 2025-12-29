## ultralytics 画图补丁


### 安装
```shell
pip install labellayout
```

### 如何使用
```python
from ultralytics import YOLO
from patch import SmartLabelPlotter
## patch.py 就在这里


plotter = SmartLabelPlotter()
plotter.register()

model = YOLO("yolo11l.pt")

model.predict("persons.jpg", save=True, imgsz=640, conf=0.2)
```


### 效果图
![效果图](result.jpg)