import cv2
import numpy as np
import threading
from functools import lru_cache
from typing import List, Dict, Any, Union

from PIL import Image, ImageDraw, ImageFont
from ultralytics.utils.plotting import Annotator
from ultralytics.engine.results import Results
from ultralytics.utils.checks import check_font

try:
    import labellayout
except ImportError:
    pass

class SmartLabelPlotter:
    """
    智能标签布局绘图器 (修复了颜色通道 BGR/RGB 错乱的问题)。
    """
    
    _lock = threading.Lock()
    
    # 原始方法占位符
    _ORIG_BOX_LABEL = None 
    _ORIG_PLOT = None

    def __init__(self, 
                 font_name: str = "Arial.ttf", 
                 padding_x: int = 3, 
                 padding_y: int = 1):
        
        self.font_path = check_font(font_name)
        
        # 布局配置
        if 'labellayout' in globals():
            self.layout_config = labellayout.LayoutConfig()
            self.layout_config.paddingX = padding_x
            self.layout_config.paddingY = padding_y
        else:
            self.layout_config = None

    @lru_cache(maxsize=128)
    def _get_pil_font(self, size: int) -> ImageFont.FreeTypeFont:
        try:
            return ImageFont.truetype(str(self.font_path), size)
        except Exception:
            return ImageFont.load_default()

    def _measure_text(self, text: str, font_size: int) -> 'labellayout.TextSize':
        font = self._get_pil_font(font_size)
        bbox = font.getbbox(text) 
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        return labellayout.TextSize(int(w), int(h), 0)

    def register(self):
        """注册补丁"""
        current_plot = Results.plot
        if hasattr(current_plot, "_is_smart_label_patch"):
            print("⚠️ SmartLabelPlotter: Patch already active. Skipping.")
            return

        if SmartLabelPlotter._ORIG_PLOT is None:
            SmartLabelPlotter._ORIG_PLOT = Results.plot
        
        if SmartLabelPlotter._ORIG_BOX_LABEL is None:
            SmartLabelPlotter._ORIG_BOX_LABEL = Annotator.box_label

        def patched_plot_proxy(result_instance, *args, **kwargs):
            return self._execute_patched_plot(result_instance, *args, **kwargs)
        
        patched_plot_proxy._is_smart_label_patch = True
        Results.plot = patched_plot_proxy
        print("🚀 SmartLabelPlotter: Patch applied successfully.")

    def unregister(self):
        """移除补丁"""
        if SmartLabelPlotter._ORIG_PLOT:
            Results.plot = SmartLabelPlotter._ORIG_PLOT
            print("🛑 SmartLabelPlotter: Patch removed.")

    def _execute_patched_plot(self, result_instance: Results, *args, **kwargs) -> Union[np.ndarray, Image.Image]:
        
        if not SmartLabelPlotter._ORIG_PLOT:
            raise RuntimeError("Original plot method is missing!")

        pending_labels: List[Dict[str, Any]] = []

        def hook_box_label(annotator_self, box, label='', color=(128, 128, 128), txt_color=(255, 255, 255), **kw):
            # 1. 调用原始方法画框（不做任何改变，让它自己处理颜色）
            if SmartLabelPlotter._ORIG_BOX_LABEL:
                SmartLabelPlotter._ORIG_BOX_LABEL(annotator_self, box, label='', color=color, txt_color=txt_color, **kw)
            
            if label:
                # 2. 判断数据源是 OpenCV 还是 PIL
                im = annotator_self.im
                is_cv2_source = isinstance(im, np.ndarray)

                # --- 关键修复：颜色空间转换 ---
                # 如果源图像是 OpenCV (numpy)，说明传入的 color 是 BGR 格式
                # 我们的渲染器后续会用 PIL (RGB) 绘制，所以必须把 BGR 转成 RGB
                final_color = color
                final_txt_color = txt_color

                if is_cv2_source:
                    # 将 (B, G, R) 转换为 (R, G, B)
                    if isinstance(color, (tuple, list)) and len(color) >= 3:
                        final_color = (color[2], color[1], color[0])
                    
                    if isinstance(txt_color, (tuple, list)) and len(txt_color) >= 3:
                        final_txt_color = (txt_color[2], txt_color[1], txt_color[0])

                # 3. 计算字体大小 (Line Width)
                if getattr(annotator_self, 'lw', None):
                    lw = annotator_self.lw
                else:
                    if is_cv2_source:
                        shape_sum = sum(im.shape[:2])
                    elif isinstance(im, Image.Image):
                        shape_sum = sum(im.size)
                    else:
                        shape_sum = 2000
                    lw = max(round(shape_sum / 2 * 0.003), 2)

                base_font_size = min(20, max(11, int(lw * 8)))
                
                # 4. 存储修正后的颜色
                pending_labels.append({
                    'box': box, 
                    'label': label, 
                    'color': final_color,        # 已经是 RGB
                    'txt_color': final_txt_color, # 已经是 RGB
                    'base_font_size': base_font_size
                })

        with self._lock:
            try:
                Annotator.box_label = hook_box_label
                final_output = SmartLabelPlotter._ORIG_PLOT(result_instance, *args, **kwargs)
            finally:
                if SmartLabelPlotter._ORIG_BOX_LABEL:
                    Annotator.box_label = SmartLabelPlotter._ORIG_BOX_LABEL

        if not pending_labels or 'labellayout' not in globals():
            return final_output

        return self._render_optimized_labels(final_output, pending_labels)

    def _render_optimized_labels(self, img_input: Union[np.ndarray, Image.Image], labels: List[Dict]) -> Union[np.ndarray, Image.Image]:
        is_cv2_input = False
        
        # 图像本身从 BGR 转 RGB
        if isinstance(img_input, np.ndarray):
            is_cv2_input = True
            im_pil = Image.fromarray(cv2.cvtColor(img_input, cv2.COLOR_BGR2RGB))
            h_img, w_img = img_input.shape[:2]
        elif isinstance(img_input, Image.Image):
            is_cv2_input = False
            im_pil = img_input
            w_img, h_img = img_input.size
        else:
            return img_input

        # 布局计算
        solver = labellayout.LabelLayout(w_img, h_img, self._measure_text, self.layout_config)
        
        for item in labels:
            b = item['box']
            solver.add(float(b[0]), float(b[1]), float(b[2]), float(b[3]), 
                       item['label'], item['base_font_size'])
        
        solver.solve()
        layout_results = solver.layout()

        # PIL 绘制 (此时 Image 是 RGB，Color 也是 RGB)
        draw = ImageDraw.Draw(im_pil)
        pad_x = self.layout_config.paddingX
        pad_y = self.layout_config.paddingY

        for i, res in enumerate(layout_results):
            info = labels[i]
            font = self._get_pil_font(res.fontSize)
            
            draw.rectangle(
                [(res.left, res.top), (res.left + res.width, res.top + res.height)], 
                fill=info['color'], 
                outline=info['color']
            )
            
            bbox_top = font.getbbox(info['label'])[1]
            text_draw_y = res.top + pad_y - bbox_top
            text_draw_x = res.left + pad_x
            
            draw.text((text_draw_x, text_draw_y), info['label'], fill=info['txt_color'], font=font)

        # 还原输出
        if is_cv2_input:
            # RGB 转回 BGR
            return cv2.cvtColor(np.asarray(im_pil), cv2.COLOR_RGB2BGR)
        else:
            return im_pil
