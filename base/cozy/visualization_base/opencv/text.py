from pathlib import Path
from typing import Tuple

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

_FONT_CANDIDATES = (
    Path(__file__).resolve().parent / 'source' / 'Hiragino Sans GB.ttc',
    Path(__file__).resolve().parent.parent / 'source' / 'Hiragino Sans GB.ttc',
    Path('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'),
    Path('/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc'),
    Path('/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc'),
    Path('/usr/share/fonts/truetype/wqy/wqy-zenhei.ttf'),
    Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),
)


def get_default_cjk_font_path() -> str:
    for font_path in _FONT_CANDIDATES:
        if font_path.exists():
            return str(font_path)
    return ''


def draw_text_with_outline(
    image: np.ndarray,
    text: str,
    position: Tuple[int, int],
    font_scale: float = 0.5,
    text_color: Tuple[int, int, int] = (255, 255, 255),
    outline_color: Tuple[int, int, int] = (0, 0, 0),
    text_thickness: int = 1,
    outline_thickness: int = 3,
    font_face: int = cv2.FONT_HERSHEY_SIMPLEX,
    line_type: int = cv2.LINE_AA,
) -> None:
    if not text:
        return

    cv2.putText(
        image,
        text,
        position,
        font_face,
        font_scale,
        outline_color,
        outline_thickness,
        lineType=line_type,
    )


def draw_unicode_text_with_outline(
        image: np.ndarray,
        text: str,
        position: Tuple[int, int],
        font_path: str = '',
        font_size: int = 16,
        text_color: Tuple[int, int, int] = (255, 255, 255),
        outline_color: Tuple[int, int, int] = (0, 0, 0),
        outline_thickness: int = 2,
) -> None:
    if not text:
        return

    selected_font_path = font_path or get_default_cjk_font_path()
    if selected_font_path:
        font = ImageFont.truetype(selected_font_path, font_size)
    else:
        font = ImageFont.load_default()

    image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    pil_image = Image.fromarray(image_rgb)
    draw = ImageDraw.Draw(pil_image)

    draw.text(
        xy=position,
        text=text,
        fill=(text_color[2], text_color[1], text_color[0]),
        font=font,
        stroke_width=outline_thickness,
        stroke_fill=(outline_color[2], outline_color[1], outline_color[0]),
    )

    rendered = cv2.cvtColor(np.array(pil_image), cv2.COLOR_RGB2BGR)
    image[:, :] = rendered
