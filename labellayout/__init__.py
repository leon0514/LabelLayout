import os
import sys

try:
    # 尝试从当前包目录导入二进制后端
    # 当 .so 文件和 __init__.py 在同一目录下时，下句有效
    from . import _labellayout_backend
    from ._labellayout_backend import TextSize, LayoutConfig, LayoutResult, LabelLayout
except ImportError as e:
    # 增加调试信息输出
    curr_dir = os.path.dirname(__file__)
    raise ImportError(
        f"\n无法加载 labellayout C++ 扩展。\n"
        f"当前包路径: {curr_dir}\n"
        f"路径内容: {os.listdir(curr_dir) if os.path.exists(curr_dir) else 'N/A'}\n"
        f"原始错误: {e}"
    )

__all__ = ["TextSize", "LayoutConfig", "LayoutResult", "LabelLayout"]