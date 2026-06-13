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
HEADER_PATH = PROJECT_DIR / "src" / "Board" / "HeadlessSetupWebPage.h"


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


import base64

def build_html():
    html = INDEX_PATH.read_text(encoding="utf-8")
    css = minify_css(read_source("style.css"))
    js = minify_js(read_source("app.js"))
    
    logo_path = PROJECT_DIR / "logo_cmas_mini.png"
    if logo_path.exists():
        logo_base64 = base64.b64encode(logo_path.read_bytes()).decode("utf-8")
        html = html.replace('logo_cmas_mini.png', f'data:image/png;base64,{logo_base64}')

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
    return f"""#ifndef HEADLESS_SETUP_WEB_PAGE_H
#define HEADLESS_SETUP_WEB_PAGE_H

#include <Arduino.h>

static constexpr const uint8_t kHeadlessSetupHtmlGzip[] PROGMEM = {{
{format_bytes(compressed)}
}};

static constexpr const size_t kHeadlessSetupHtmlGzipSize = sizeof(kHeadlessSetupHtmlGzip);
static constexpr const size_t kHeadlessSetupHtmlOriginalSize = {len(html.encode("utf-8"))};

#endif
"""


def generate():
    header = make_header(build_html())
    previous = HEADER_PATH.read_text(encoding="utf-8") if HEADER_PATH.exists() else None
    if previous != header:
        HEADER_PATH.write_text(header, encoding="utf-8")
        print(f"Generated {HEADER_PATH.relative_to(PROJECT_DIR)}")


generate()
