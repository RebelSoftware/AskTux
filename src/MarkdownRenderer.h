#ifndef LINHELP_MARKDOWN_RENDERER_H
#define LINHELP_MARKDOWN_RENDERER_H

#include <string>

/**
 * MarkdownRenderer — converts Markdown to styled HTML using cmark.
 *
 * The CSS is loaded from an external file at runtime so users can
 * customise the appearance without recompiling.
 */
class MarkdownRenderer {
public:
    MarkdownRenderer();

    /**
     * Convert markdown text to a complete HTML document suitable for
     * display in a WebKitWebView.
     */
    std::string to_html(const std::string& markdown) const;

    /**
     * Load CSS from a file.  Called automatically during construction
     * from the default install location, but can be called again to
     * hot-reload styles.
     */
    void load_css(const std::string& path);

    /** Return the currently loaded CSS text. */
    const std::string& css() const { return css_; }

private:
    std::string css_;
    std::string html_prefix_;   // <!DOCTYPE html>…<style>…</style><body>
    std::string html_suffix_;   // </body></html> + auto-scroll script

    /** Rebuild prefix/suffix from the current CSS. */
    void rebuild_template();
};

#endif // LINHELP_MARKDOWN_RENDERER_H
