#include "MainWindow.h"
#include "SettingsDialog.h"
#include "Config.h"
#include "SystemInfo.h"
#include "PromptBuilder.h"
#include "LLMClient.h"
#include "MarkdownRenderer.h"
#include "ScopedTimer.h"
#include "Tool.h"

#include <glibmm.h>
#include <iostream>
#include <sstream>
#include <regex>
#include <thread>
#include <sstream>

// ── Constructor ──────────────────────────────────────────────────────────────
MainWindow::MainWindow()
{
    set_title("AskTux — Linux Help Assistant");
    set_default_size(720, 620);

    // ── HTML output area (WebKitWebView) ─────────────────────────────────────
    md_renderer_ = std::make_unique<MarkdownRenderer>();

    // Create the WebKitWebView (C API — no gtkmm wrapper available).
    webview_ = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webview_widget_ = Glib::wrap(GTK_WIDGET(webview_));
    webview_widget_->set_hexpand(true);
    webview_widget_->set_vexpand(true);

    // Disable context menu for a cleaner reading experience.
    auto* settings = webkit_web_view_get_settings(webview_);
    webkit_settings_set_enable_developer_extras(settings, false);

    // Load a blank page to start.
    webkit_web_view_load_html(webview_, "<html><body></body></html>", nullptr);

    output_scroll_.set_child(*webview_widget_);
    output_scroll_.set_expand(true);
    output_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    // ── Progress bar (hidden by default) ────────────────────────────────────
    progress_bar_.set_show_text(true);
    progress_bar_.set_visible(false);

    // ── Question input (multiline) + Submit + Cancel ─────────────────────────
    question_scroll_.set_min_content_height(60);
    question_scroll_.set_max_content_height(150);
    question_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    question_scroll_.set_child(question_view_);
    question_view_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    question_view_.set_hexpand(true);

    // Ctrl+Enter to submit.
    auto ctrl_enter = Gtk::EventControllerKey::create();
    ctrl_enter->signal_key_pressed().connect([this](guint keyval, guint, Gdk::ModifierType state) {
        if ((state & Gdk::ModifierType::CONTROL_MASK) != Gdk::ModifierType(0)
            && keyval == GDK_KEY_Return) {
            on_submit();
            return true;
        }
        return false;
    }, true);
    question_view_.add_controller(ctrl_enter);

    auto entry_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    entry_box->append(question_scroll_);
    submit_btn_.set_label("Submit");
    cancel_btn_.set_label("Cancel");
    cancel_btn_.set_visible(false);

    entry_box->append(submit_btn_);
    entry_box->append(cancel_btn_);

    // ── Bottom bar: Copy, Settings, Spinner, Status ──────────────────────────
    auto bottom_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);

    copy_btn_.set_label("Copy");
    settings_btn_.set_label("Settings");
    spinner_.set_size_request(20, 20);
    status_label_.set_text("Ready");

    bottom_box->append(copy_btn_);
    bottom_box->append(settings_btn_);
    bottom_box->append(spinner_);
    bottom_box->append(status_label_);

    // ── Main layout ──────────────────────────────────────────────────────────
    main_box_.set_spacing(8);
    main_box_.set_margin(12);
    main_box_.append(output_scroll_);
    main_box_.append(progress_bar_);
    main_box_.append(*entry_box);
    main_box_.append(*bottom_box);

    set_child(main_box_);

    // ── Signals ──────────────────────────────────────────────────────────────
    submit_btn_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_submit));
    cancel_btn_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_cancel));
    copy_btn_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_copy));
    settings_btn_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_settings));

    dispatcher_.connect(sigc::mem_fun(*this, &MainWindow::on_dispatcher_tick));
}

MainWindow::~MainWindow() = default;

// ── Submit ───────────────────────────────────────────────────────────────────
void MainWindow::on_submit()
{
    auto buf = question_view_.get_buffer();
    std::string question = buf->get_text(false);
    if (question.empty()) return;

    clear_output();
    set_busy(true);
    status_label_.set_text("Connecting…");

    const auto& cfg = Config::instance();
    std::string cfg_error = cfg.validate();
    if (!cfg_error.empty()) {
        // Show configuration errors as HTML too.
        raw_output_ = "**Configuration error:**\n\n  " + cfg_error +
                      "\n\nOpen *Settings* to fix it.";
        update_html_content();
        set_busy(false);
        status_label_.set_text("Ready");
        return;
    }

    ScopedTimer setup_timer("MainWindow::on_submit setup (SystemInfo + PromptBuilder + LLMClient::create)");
    const SystemInfo& info = SystemInfo::get();
    std::string system_prompt = PromptBuilder::build(
        cfg.system_prompt_template(), info, question);

    client_ = LLMClient::create();
    setup_timer.lap("Setup complete — about to call client_->send_request()");
    streaming_finished_ = false;
    first_token_received_ = false;
    request_start_ = std::chrono::steady_clock::now();

    // Store for potential tool-execution continuation.
    last_question_ = question;
    last_system_prompt_ = system_prompt;
    tool_continuation_depth_ = 0;

    progress_bar_.set_visible(false);
    progress_bar_.set_fraction(0.0);
    cancel_btn_.set_visible(true);
    submit_btn_.set_visible(false);

    auto tq_ptr = &token_queue_;
    auto tm_ptr = &token_mutex_;
    auto pq_ptr = &progress_queue_;
    auto pm_ptr = &progress_mutex_;
    auto disp   = &dispatcher_;
    auto fin    = &streaming_finished_;

    client_->send_request(
        system_prompt, question,

        [tq_ptr, tm_ptr, disp](const std::string& token) {
            { std::lock_guard<std::mutex> lock(*tm_ptr); tq_ptr->push(token); }
            disp->emit();
        },
        [pq_ptr, pm_ptr, disp](const std::string& status) {
            { std::lock_guard<std::mutex> lock(*pm_ptr); pq_ptr->push(status); }
            disp->emit();
        },
        [disp, tm_ptr, tq_ptr, fin](const std::string& err) {
            { std::lock_guard<std::mutex> lock(*tm_ptr);
              tq_ptr->push("\n\n**Error:** " + err); }
            *fin = true;
            disp->emit();
        },
        [this, fin]() { *fin = true; dispatcher_.emit(); }
    );
}

// ── Cancel ───────────────────────────────────────────────────────────────────
void MainWindow::on_cancel()
{
    if (client_) {
        client_->cancel();   // sets atomic<bool> cancelled_ = true
    }
    streaming_finished_ = true;
    status_label_.set_text("Cancelled");
    cancel_btn_.set_visible(false);
    progress_bar_.set_visible(false);
    set_busy(false);
}

// ── Dispatcher tick (main thread) ────────────────────────────────────────────
void MainWindow::on_dispatcher_tick()
{
    drain_progress_queue();
    drain_token_queue();
}

void MainWindow::drain_token_queue()
{
    bool got_token = false;
    while (true) {
        std::string token;
        {
            std::lock_guard<std::mutex> lock(token_mutex_);
            if (token_queue_.empty()) break;
            token = token_queue_.front();
            token_queue_.pop();
        }

        if (!got_token && !first_token_received_) {
            first_token_received_ = true;
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - request_start_).count();
            status_label_.set_text("TTFT: " + std::to_string(ms) + "ms");
        }
        got_token = true;
        append_output(token);
    }

    if (streaming_finished_) {
        if (first_token_received_) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - request_start_).count();
            status_label_.set_text("Done (" + std::to_string(ms) + "ms)");
        } else {
            status_label_.set_text("Ready");
        }

        // Check if the response contains a tool call we should execute.
        bool handled = false;
        if (first_token_received_ && tool_continuation_depth_ < 3)
            handled = check_and_execute_tool();

        if (!handled) {
            cancel_btn_.set_visible(false);
            submit_btn_.set_visible(true);
            progress_bar_.set_visible(false);
            set_busy(false);
        }
    }
}

void MainWindow::drain_progress_queue()
{
    while (true) {
        std::string status;
        {
            std::lock_guard<std::mutex> lock(progress_mutex_);
            if (progress_queue_.empty()) break;
            status = progress_queue_.front();
            progress_queue_.pop();
        }

        progress_bar_.set_visible(true);
        progress_bar_.set_text(status);

        auto paren = status.rfind(" (");
        if (paren != std::string::npos) {
            auto pct_str = status.substr(paren + 2);
            pct_str = pct_str.substr(0, pct_str.size() - 1);
            try {
                double pct = std::stod(pct_str) / 100.0;
                progress_bar_.set_fraction(pct);
            } catch (...) { progress_bar_.pulse(); }
        } else {
            progress_bar_.pulse();
        }
        status_label_.set_text(status);
    }
}

// ── Tool execution ───────────────────────────────────────────────────────────
bool MainWindow::check_and_execute_tool()
{
    std::string tool_name, tool_args;
    if (!ToolRegistry::instance().parse_tool_call(raw_output_, tool_name, tool_args))
        return false;  // no tool call found

    std::cerr << "[AskTux] Tool call detected: " << tool_name
              << " args=\"" << tool_args << "\"" << std::endl;
    status_label_.set_text("Running " + tool_name + "…");
    last_tool_name_ = tool_name;

    // Remove the tool call marker AND any AI preview text before it.
    // raw_output_ contains only AI response tokens, so everything before
    // the tool call is preamble that should not be shown.
    {
        std::regex tool_re(R"(\[TOOL:\s+\w+(?:\s+args=\"[^\"]*\")?\])");
        std::smatch match;
        if (std::regex_search(raw_output_, match, tool_re)) {
            // Erase from the start of the response to the end of the tool call.
            raw_output_.erase(0, match.position() + match.length());
        }
    }

    // Show a collapsed placeholder in the output.
    raw_output_ += "\n\n> **🔧 [tool] Running tool:** " + tool_name + "…\n>\n"
                   "> _(waiting for result…)_\n\n";
    update_html_content();

    // Execute the tool on a background thread.
    std::thread([this, tool_name, tool_args]() {
        std::string result = ToolRegistry::instance().execute(tool_name, tool_args);

        // Replace the placeholder with the actual result (on main thread).
        Glib::signal_idle().connect_once([this, tool_name, result]() {
            // Remove the placeholder.
            auto pos = raw_output_.rfind("> **🔧 [tool] Running tool:");
            if (pos != std::string::npos) {
                // Find the blank line after the placeholder.
                auto end = raw_output_.find("\n\n", pos + 1);
                if (end != std::string::npos) end += 2;
                else end = raw_output_.size();
                raw_output_.erase(pos, end - pos);
            }
            // Inject the final result as a tagged blockquote.
            raw_output_ += "\n\n> **🔧 [tool] " + tool_name + "**\n> \n> ```\n";
            // Indent each line of the result so it stays inside the blockquote.
            {
                std::istringstream stream(result);
                std::string line;
                while (std::getline(stream, line)) {
                    raw_output_ += "> " + line + "\n";
                }
            }
            raw_output_ += "> ```\n\n";
            update_html_content();

            // Make a second request with the tool result as context.
            ++tool_continuation_depth_;
            status_label_.set_text("Continuing with tool result…");
            do_tool_continuation();
        });
    }).detach();
    return true;
}

void MainWindow::do_tool_continuation()
{
    // The tool result is already shown to the user as a blockquote in the
    // output area.  For the continuation we just tell the model to continue
    // its response — no need to repeat the raw output.
    std::string continuation =
        "[System instruction: The \"" + last_tool_name_
        + "\" tool was executed above. Its output has been displayed to the "
        "user. Continue your response naturally — directly answer the user's "
        "original question based on that information. Do not describe what "
        "the tool returned or repeat the tool output.]";

    streaming_finished_ = false;
    first_token_received_ = false;
    request_start_ = std::chrono::steady_clock::now();
    progress_bar_.set_visible(false);
    progress_bar_.set_fraction(0.0);

    auto tq_ptr = &token_queue_;
    auto tm_ptr = &token_mutex_;
    auto pq_ptr = &progress_queue_;
    auto pm_ptr = &progress_mutex_;
    auto disp   = &dispatcher_;
    auto fin    = &streaming_finished_;

    client_ = LLMClient::create();
    client_->send_request(
        last_system_prompt_, continuation,

        [tq_ptr, tm_ptr, disp](const std::string& token) {
            { std::lock_guard<std::mutex> lock(*tm_ptr); tq_ptr->push(token); }
            disp->emit();
        },
        [pq_ptr, pm_ptr, disp](const std::string& status) {
            { std::lock_guard<std::mutex> lock(*pm_ptr); pq_ptr->push(status); }
            disp->emit();
        },
        [disp, tm_ptr, tq_ptr, fin](const std::string& err) {
            { std::lock_guard<std::mutex> lock(*tm_ptr);
              tq_ptr->push("\n\n**Error during tool continuation:** " + err); }
            *fin = true;
            disp->emit();
        },
        [this, fin]() { *fin = true; dispatcher_.emit(); }
    );
}

// ── Copy ─────────────────────────────────────────────────────────────────────
void MainWindow::on_copy()
{
    auto clipboard = Gtk::Widget::get_clipboard();
    clipboard->set_text(raw_output_);
}

// ── Settings ─────────────────────────────────────────────────────────────────
void MainWindow::on_settings()
{
    auto dialog = std::make_unique<SettingsDialog>(*this);
    dialog->set_modal(true);
    auto* ptr = dialog.release();
    ptr->signal_response().connect([ptr](int) { delete ptr; });
    ptr->show();
}

// ── UI helpers ───────────────────────────────────────────────────────────────
void MainWindow::set_busy(bool busy)
{
    submit_btn_.set_sensitive(!busy);
    submit_btn_.set_visible(!busy);
    question_view_.set_sensitive(!busy);
    if (busy) spinner_.start(); else spinner_.stop();
}

void MainWindow::append_output(const std::string& text)
{
    raw_output_ += text;
    // Convert the entire accumulated markdown to HTML and display it.
    // cmark is fast enough for this on every token — and it means the
    // user sees properly formatted HTML *as the stream arrives*, not
    // just after the final token.
    update_html_content();
}

void MainWindow::clear_output()
{
    raw_output_.clear();
    update_html_content();
}

void MainWindow::update_html_content()
{
    if (raw_output_.empty()) {
        webkit_web_view_load_html(webview_,
            "<html><body></body></html>", nullptr);
        return;
    }

    std::string html = md_renderer_->to_html(raw_output_);
    webkit_web_view_load_html(webview_, html.c_str(), nullptr);
}
