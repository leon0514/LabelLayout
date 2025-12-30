import os
from setuptools import setup, find_packages

# 读取 README.md 的内容（如果存在）
long_description = ""
if os.path.exists("README.md"):
    with open("README.md", encoding="utf-8") as f:
        long_description = f.read()

setup(
    name="smart-label-plotter",          
    version="0.0.2",                     
    author="Leon",                  
    description="A patch for Ultralytics YOLO to optimize label layout using labellayout.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=["smart_label_plotter"],           
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.8',             
    install_requires=[                   
        "numpy",
        "opencv-python",
        "Pillow",
        "ultralytics",
        "labellayout",  # 如果 labellayout 是私有包，请保持注释；如果是 PyPI 包，请取消注释
    ],
)