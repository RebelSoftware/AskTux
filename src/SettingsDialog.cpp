#include "SettingsDialog.h"
#include "Config.h"
#include "OllamaClient.h"

SettingsDialog::SettingsDialog(Gtk::Window& parent)
    : Gtk::Dialog("Settings", parent)
{
    set_default_size(500, 500);

    auto* content = get_content_area();
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_column_spacing(12);
    grid->set_row_spacing(8);
    grid->set_margin(12);
    content->append(*grid);

    int row = 0;

    // Backend
    grid->attach(*Gtk::make_managed<Gtk::Label>("Backend", Gtk::Align::START), 0, row, 1, 1);
    backend_combo_.append("ollama");
    backend_combo_.append("openai");
    grid->attach(backend_combo_, 1, row, 1, 1);
    ++row;

    // Model — selector + entry side by side
    grid->attach(*Gtk::make_managed<Gtk::Label>("Model", Gtk::Align::START), 0, row, 1, 1);
    auto model_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    model_box->append(model_combo_);
    model_entry_.set_hexpand(true);
    model_entry_.set_placeholder_text("Or type a custom model…");
    model_box->append(model_entry_);
    refresh_models_btn_.set_label("⟳");
    refresh_models_btn_.set_tooltip_text("Refresh installed models from Ollama");
    model_box->append(refresh_models_btn_);
    grid->attach(*model_box, 1, row, 1, 1);
    ++row;

    // Ollama URL
    grid->attach(*Gtk::make_managed<Gtk::Label>("Ollama URL", Gtk::Align::START), 0, row, 1, 1);
    grid->attach(ollama_url_entry_, 1, row, 1, 1);
    ++row;

    // OpenAI URL
    grid->attach(*Gtk::make_managed<Gtk::Label>("OpenAI URL", Gtk::Align::START), 0, row, 1, 1);
    grid->attach(openai_url_entry_, 1, row, 1, 1);
    ++row;

    // API Key
    grid->attach(*Gtk::make_managed<Gtk::Label>("API Key", Gtk::Align::START), 0, row, 1, 1);
    api_key_entry_.set_visibility(false);
    grid->attach(api_key_entry_, 1, row, 1, 1);
    ++row;

    // System Prompt
    grid->attach(*Gtk::make_managed<Gtk::Label>("System Prompt", Gtk::Align::START),
                 0, row, 1, 1);
    prompt_scroll_.set_min_content_height(180);
    prompt_scroll_.set_child(prompt_textview_);
    grid->attach(prompt_scroll_, 1, row, 1, 1);
    ++row;

    // ── Buttons ──────────────────────────────────────────────────────────────
    add_button("Cancel", Gtk::ResponseType::CANCEL);
    add_button("Save",   Gtk::ResponseType::OK);

    // ── Load ─────────────────────────────────────────────────────────────────
    populate_from_config();
    populate_ollama_models();

    // ── Signals ──────────────────────────────────────────────────────────────
    signal_response().connect(sigc::mem_fun(*this, &SettingsDialog::on_save));
    backend_combo_.signal_changed().connect(
        sigc::mem_fun(*this, &SettingsDialog::on_backend_changed));
    model_combo_.signal_changed().connect([this]() {
        // When an installed model is selected from the dropdown, copy to entry.
        std::string sel = model_combo_.get_active_text();
        if (!sel.empty())
            model_entry_.set_text(sel);
    });
    refresh_models_btn_.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsDialog::populate_ollama_models));
}

void SettingsDialog::populate_from_config()
{
    const auto& cfg = Config::instance();

    backend_combo_.set_active_text(cfg.backend());
    if (backend_combo_.get_active_row_number() < 0)
        backend_combo_.set_active(0);

    model_entry_.set_text(cfg.model());
    model_combo_.set_active_text(cfg.model());

    ollama_url_entry_.set_text(cfg.ollama_url());
    openai_url_entry_.set_text(cfg.openai_url());
    api_key_entry_.set_text(cfg.openai_key());

    auto buf = prompt_textview_.get_buffer();
    buf->set_text(cfg.system_prompt_template());

    on_backend_changed();
}

void SettingsDialog::populate_ollama_models()
{
    auto models = OllamaClient::list_models();
    if (models.empty()) return;

    std::string current = model_entry_.get_text();

    // Rebuild the dropdown.
    while (model_combo_.get_model()->children().size() > 0)
        model_combo_.remove_text(0);

    for (const auto& m : models)
        model_combo_.append(m);

    // Restore entry text (dropdown selection is separate).
    model_entry_.set_text(current);
}

void SettingsDialog::on_backend_changed()
{
    bool is_ollama = (backend_combo_.get_active_text() == "ollama");
    ollama_url_entry_.set_sensitive(is_ollama);
    model_combo_.set_sensitive(is_ollama);
    model_entry_.set_sensitive(is_ollama);
    refresh_models_btn_.set_sensitive(is_ollama);
    openai_url_entry_.set_sensitive(!is_ollama);
    api_key_entry_.set_sensitive(!is_ollama);
}

void SettingsDialog::on_save(int response)
{
    if (response != Gtk::ResponseType::OK) return;

    auto& cfg = Config::instance();
    cfg.set_backend(backend_combo_.get_active_text());

    // Use the entry text (user can type any model name).
    std::string model = model_entry_.get_text();
    if (model.empty())
        model = model_combo_.get_active_text();
    cfg.set_model(model);

    cfg.set_ollama_url(ollama_url_entry_.get_text());
    cfg.set_openai_url(openai_url_entry_.get_text());
    cfg.set_openai_key(api_key_entry_.get_text());
    cfg.set_system_prompt_template(prompt_textview_.get_buffer()->get_text());

    cfg.save();
}
