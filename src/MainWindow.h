#ifndef asktux_MAIN_WINDOW_H
#define asktux_MAIN_WINDOW_H

#include <gtkmm.h>
#include <webkit/webkit.h>
#include <memory>
#include <queue>
#include <mutex>
#include <chrono>

class LLMClient;
class MarkdownRenderer;

/**
 * MainWindow — the primary application window.
 *
 * Layout:
 *   +-----------------------------------------------+
 *   |  Output Area (WebKitWebView, HTML rendering)   |
 *   |                                                |
 *   |  [thread-safe token queue ← background thread] |
 *   +-----------------------------------------------+
 *   |  Progress bar (hidden when idle)               |
 *   |  Question Entry (Gtk::Entry)   [Submit][Cancel]|
 *   |  [Copy] [Settings]   Status: spinner/label     |
 *   +-----------------------------------------------+
 *
 * Markdown from the LLM is converted to HTML via cmark on every
 * token arrival and displayed in a WebKitWebView.  An external CSS
 * file (style.css) allows post-compilation styling changes.
 */
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    ~MainWindow() override;

private:
    // ── Signal handlers ──────────────────────────────────────────────────────
    void on_submit();
    void on_cancel();
    void on_copy();
    void on_settings();
    void on_dispatcher_tick();

    // ── Tool execution (post-stream) ─────────────────────────────────────────
    /** Returns true if a tool call was found and executed. */
    bool check_and_execute_tool();
    void do_tool_continuation();

    // ── Drain queues ─────────────────────────────────────────────────────────
    void drain_token_queue();
    void drain_progress_queue();

    // ── UI state helpers ─────────────────────────────────────────────────────
    void set_busy(bool busy);
    void append_output(const std::string& text);
    void clear_output();
    void update_html_content();

    // ── Widgets ──────────────────────────────────────────────────────────────
    WebKitWebView*      webview_ = nullptr;     // raw C pointer (no gtkmm wrapper)
    Gtk::Widget*        webview_widget_ = nullptr; // Gtk::Widget wrapper for layout
    Gtk::ScrolledWindow output_scroll_;

    // ── Markdown / HTML rendering ────────────────────────────────────────────
    std::unique_ptr<MarkdownRenderer> md_renderer_;
    std::string raw_output_;   // accumulated markdown

    Gtk::ScrolledWindow question_scroll_;
    Gtk::TextView       question_view_;
    Gtk::Button         submit_btn_;
    Gtk::Button         cancel_btn_;
    Gtk::Button         copy_btn_;
    Gtk::Button         settings_btn_;
    Gtk::Spinner        spinner_;
    Gtk::Label          status_label_;
    Gtk::ProgressBar    progress_bar_;
    Gtk::Box            main_box_{Gtk::Orientation::VERTICAL};

    // ── Thread-safe queues ───────────────────────────────────────────────────
    std::queue<std::string> token_queue_;
    std::mutex              token_mutex_;
    std::queue<std::string> progress_queue_;
    std::mutex              progress_mutex_;

    // ── Glib::Dispatcher to marshal data to the main thread ──────────────────
    Glib::Dispatcher dispatcher_;

    // ── Current LLM client ───────────────────────────────────────────────────
    std::unique_ptr<LLMClient> client_;
    std::atomic<bool> streaming_finished_{true};

    // ── Timing ───────────────────────────────────────────────────────────────
    std::chrono::steady_clock::time_point request_start_;
    bool first_token_received_ = false;

    // ── Tool execution state ─────────────────────────────────────────────────
    std::string last_question_;      // the original user question
    std::string last_system_prompt_; // the system prompt used
    std::string last_tool_name_;     // name of the tool being executed
    int tool_continuation_depth_ = 0; // prevent infinite tool loops
};

#endif // asktux_MAIN_WINDOW_H
