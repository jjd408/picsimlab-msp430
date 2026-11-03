"""Turns SVG <text> elements into glyph outlines, for PICSimLab's SVG renderer, which doesn't draw text.

    python3 svgtext.py input.svg output.svg

Handles the simple <text> this project writes: x, y, font-size, fill, text-anchor (start/middle/end) and plain
text content, set in DejaVu Sans.
"""
import re
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def text_path(font, text, x, y, size, anchor):
    glyphs = font.getGlyphSet()
    cmap = font.getBestCmap()
    scale = size / font["head"].unitsPerEm
    names = [cmap.get(ord(c), ".notdef") for c in text]
    width = sum(font["hmtx"][n][0] for n in names) * scale
    if anchor == "middle":
        x -= width / 2
    elif anchor == "end":
        x -= width
    pen = SVGPathPen(glyphs)
    cursor = 0
    for n in names:
        # Font units have y up; flip into SVG's y-down coordinates at the baseline
        glyphs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x + cursor * scale, y)))
        cursor += font["hmtx"][n][0]
    return pen.getCommands()


def convert(svg):
    font = TTFont(FONT)

    def replace(m):
        attrs, content = m.group(1), m.group(2)
        get = lambda k, d=None: (re.search(r'\b' + k + r'="([^"]*)"', attrs) or [None, d])[1]
        d = text_path(font, content, float(get("x", 0)), float(get("y", 0)), float(get("font-size", 10)),
                      get("text-anchor", "start"))
        return f'<path d="{d}" fill="{get("fill", "#000")}"/>'

    return re.sub(r"<text([^>]*)>([^<]*)</text>", replace, svg)


if __name__ == "__main__":
    src = open(sys.argv[1]).read()
    open(sys.argv[2], "w").write(convert(src))
