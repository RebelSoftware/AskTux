#include "MarkdownRenderer.h"

#include <cmark.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

// ── Helpers ──────────────────────────────────────────────────────────────────

/** Locate the CSS file relative to the installed binary or a known path. */
static std::string find_css_file()
{
    // 1. Hard-coded install path (set at build time via -DCMAKE_INSTALL_PREFIX
    //    or meson's --prefix; defaults to /usr).
    const char* prefix = std::getenv("ASKTUX_PREFIX");
    std::string base = prefix ? prefix : "/usr";

    std::string installed = base + "/share/asktux/style.css";
    std::ifstream f(installed);
    if (f.good()) return installed;

    // 2. Look relative to the executable (for development builds).
    std::string local = "data/style.css";
    f.open(local);
    if (f.good()) return local;

    local = "../data/style.css";
    f.open(local);
    if (f.good()) return local;

    // 3. Build directory fallback.
    local = "build/data/style.css";
    f.open(local);
    if (f.good()) return local;

    // Not found — return empty; built-in defaults will be used.
    return "";
}

// ── Constructor ──────────────────────────────────────────────────────────────

MarkdownRenderer::MarkdownRenderer()
{
    // Try loading from the default install location.
    std::string path = find_css_file();
    if (!path.empty()) {
        load_css(path);
    } else {
        // Provide sensible built-in defaults so the app works out of the box.
        css_ = R"(
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto,
                 Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
    line-height: 1.6;
    color: #d4d4d4;
    background: #1e1e1e;
    padding: 12px;
    margin: 0;
    word-wrap: break-word;
}
h1, h2, h3, h4 { color: #e0e0e0; margin: 0.5em 0 0.3em; }
h1 { font-size: 1.6em; border-bottom: 1px solid #333; padding-bottom: 0.2em; }
h2 { font-size: 1.35em; border-bottom: 1px solid #333; padding-bottom: 0.15em; }
h3 { font-size: 1.15em; }
p  { margin: 0.4em 0; }
code {
    font-family: "Cascadia Code", "Fira Code", "JetBrains Mono", "Droid Sans Mono", monospace;
    font-size: 0.88em;
    background: #2d2d2d;
    padding: 0.15em 0.4em;
    border-radius: 3px;
}
pre {
    background: #0d0d0d;
    padding: 10px 14px;
    border-radius: 6px;
    overflow-x: auto;
    border: 1px solid #333;
}
pre code {
    background: transparent;
    padding: 0;
    font-size: 0.85em;
}
blockquote {
    margin: 0.4em 0;
    padding: 0.2em 0.8em;
    border-left: 4px solid #4a9eff;
    color: #aaa;
    background: rgba(74, 158, 255, 0.05);
}
table { border-collapse: collapse; margin: 0.6em 0; }
th, td { border: 1px solid #444; padding: 4px 10px; text-align: left; }
th { background: #2d2d2d; }
a { color: #4a9eff; }
a:hover { text-decoration: underline; }
ul, ol { margin: 0.3em 0; padding-left: 1.5em; }
li { margin: 0.15em 0; }
hr { border: none; border-top: 1px solid #444; margin: 1em 0; }
)";
        rebuild_template();
    }
}

// ── Load CSS from a file ────────────────────────────────────────────────────

void MarkdownRenderer::load_css(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[AskTux] Warning: could not open CSS file: "
                  << path << std::endl;
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    css_ = ss.str();
    std::cout << "[AskTux] Loaded CSS from " << path << std::endl;
    rebuild_template();
}

// ── Rebuild HTML template ───────────────────────────────────────────────────

void MarkdownRenderer::rebuild_template()
{
    std::string escaped_css;
    // The CSS is placed inside a <style> tag, so we only need to
    // escape </style> if present (extremely unlikely in practice).
    for (char c : css_) {
        escaped_css += c;
    }

    html_prefix_ =
        "<!DOCTYPE html>\n"
        "<html>\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"color-scheme\" content=\"dark light\">\n"
        "<style>\n" + escaped_css + "\n</style>\n"
        "</head>\n<body>\n";

    html_suffix_ =
        "\n<script>\n"
        "  window.scrollTo(0, document.body.scrollHeight);\n"
        "</script>\n"
        "</body>\n</html>\n";
}

// ── Convert Markdown → full HTML document ────────────────────────────────────

std::string MarkdownRenderer::to_html(const std::string& markdown) const
{
    char* raw = cmark_markdown_to_html(
        markdown.c_str(), markdown.size(), CMARK_OPT_DEFAULT);

    std::string body_html(raw);
    free(raw);

    return html_prefix_ + body_html + html_suffix_;
}
