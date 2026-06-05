from pathlib import Path
import gzip
import re

try:
    Import("env")  # type: ignore[name-defined]
except NameError:
    env = None

PROJECT_DIR = Path(env.subst("$PROJECT_DIR")) if env else Path(__file__).resolve().parents[1]
WEB_DIR = PROJECT_DIR / "web-server"
INDEX_PATH = WEB_DIR / "index.html"
HEADER_PATH = PROJECT_DIR / "src" / "Board" / "DroneSetupWebPage.h"


def read_source(name):
    return (WEB_DIR / name).read_text(encoding="utf-8")


def minify_css(css):
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    css = re.sub(r"\s+", " ", css)
    css = re.sub(r"\s*([{}:;,>])\s*", r"\1", css)
    return css.strip()


def minify_js(js):
    js = re.sub(r"/\*.*?\*/", "", js, flags=re.S)
    js = re.sub(r"^[ \t]*//.*$", "", js, flags=re.M)
    js = re.sub(r"^[ \t]+", "", js, flags=re.M)
    js = re.sub(r"\n{2,}", "\n", js)
    js = re.sub(r"\s*([{}();,:=<>+*?/|-])\s*", r"\1", js)
    return js.strip()


def minify_html(html):
    html = re.sub(r">\s+<", "><", html)
    html = re.sub(r"\n{2,}", "\n", html)
    return html.strip()


def build_html():
    html = INDEX_PATH.read_text(encoding="utf-8")
    css = minify_css(read_source("style.css"))
    js = minify_js(read_source("app.js"))
    html = re.sub(
        r'<link\s+rel=["\']stylesheet["\']\s+href=["\']style\.css["\']\s*/?>',
        lambda _: f"<style>{css}</style>",
        html,
        count=1,
    )
    html = re.sub(
        r'<script\s+src=["\']app\.js["\']\s*>\s*</script>',
        lambda _: f"<script>\n{js}\n</script>",
        html,
        count=1,
    )
    return minify_html(html)


def format_bytes(data):
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        lines.append("  " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(lines)


def gzip_html(html):
    return gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)


def make_header(html):
    compressed = gzip_html(html)
    return f"""#ifndef DRONE_SETUP_WEB_PAGE_H
#define DRONE_SETUP_WEB_PAGE_H

#include <Arduino.h>

static constexpr const uint8_t kDroneSetupHtmlGzip[] PROGMEM = {{
{format_bytes(compressed)}
}};

static constexpr const size_t kDroneSetupHtmlGzipSize = sizeof(kDroneSetupHtmlGzip);
static constexpr const size_t kDroneSetupHtmlOriginalSize = {len(html.encode("utf-8"))};

#endif
"""


def generate():
    header = make_header(build_html())
    previous = HEADER_PATH.read_text(encoding="utf-8") if HEADER_PATH.exists() else None
    if previous != header:
        HEADER_PATH.write_text(header, encoding="utf-8")
        print(f"Generated {HEADER_PATH.relative_to(PROJECT_DIR)}")


generate()
