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
HEADER_PATH = PROJECT_DIR / "src" / "Headless" / "HeadlessSetupWebPage.h"
LOGO_HREF = "logo_cmas_mini.png"
LOGO_PATH = WEB_DIR / LOGO_HREF


def read_source(name):
    return (WEB_DIR / name).read_text(encoding="utf-8")


def minify_css(css):
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    css = re.sub(r"\s+", " ", css)
    css = re.sub(r"\s*([{}:;,>])\s*", r"\1", css)
    return css.strip()


def minify_js(js):
    js = re.sub(r"/\*.*?\*/", "", js, flags=re.S)
    lines = []
    for line in js.splitlines():
        line_clean = re.sub(r"\s*//(?![^\'\"`]*[\'\"`]).*$", "", line)
        if line_clean.strip():
            lines.append(line_clean)
    return "\n".join(lines)


def minify_html(html):
    html = re.sub(r">\s+<", "><", html)
    html = re.sub(r"\n{2,}", "\n", html)
    return html.strip()


import base64

def build_html():
    html = INDEX_PATH.read_text(encoding="utf-8")
    css = minify_css(read_source("style.css"))
    js = minify_js(read_source("app.js"))
    
    logo_base64 = base64.b64encode(LOGO_PATH.read_bytes()).decode("utf-8")
    html = html.replace(LOGO_HREF, f"data:image/png;base64,{logo_base64}")

    html = replace_once(
        r'<link\s+rel=["\']stylesheet["\']\s+href=["\']style\.css["\']\s*/?>',
        f"<style>{css}</style>",
        html,
    )
    html = replace_once(
        r'<script\s+src=["\']app\.js["\']\s*>\s*</script>',
        f"<script>\n{js}\n</script>",
        html,
    )
    return minify_html(html)


def replace_once(pattern, replacement, html):
    # Si index.html cambia y el patrón deja de calzar, la página quedaría sin
    # CSS o JS; mejor fallar el build.
    html, count = re.subn(pattern, lambda _: replacement, html, count=1)
    if count != 1:
        raise RuntimeError(f"index.html no contiene el patrón esperado: {pattern}")
    return html


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


def header_html(header):
    # Distintas versiones de zlib comprimen distinto el mismo contenido, así que
    # se compara el HTML descomprimido para no reescribir el header sin motivo.
    hex_bytes = re.findall(r"0x([0-9a-f]{2})", header.split("PROGMEM", 1)[-1])
    try:
        return gzip.decompress(bytes(int(b, 16) for b in hex_bytes)).decode("utf-8")
    except (OSError, EOFError, UnicodeDecodeError):
        return None


def generate():
    html = build_html()
    previous = HEADER_PATH.read_text(encoding="utf-8") if HEADER_PATH.exists() else None
    if previous is None or header_html(previous) != html:
        HEADER_PATH.write_text(make_header(html), encoding="utf-8")
        print(f"Generated {HEADER_PATH.relative_to(PROJECT_DIR)}")


generate()
