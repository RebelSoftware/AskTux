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

    // Provider
    grid->attach(*Gtk::make_managed<Gtk::Label>("Provider", Gtk::Align::START), 0, row, 1, 1);
    grid->attach(provider_combo_, 1, row, 1, 1);
    ++row;

    // API Key (shown/hidden based on provider)
    grid->attach(*Gtk::make_managed<Gtk::Label>("API Key", Gtk::Align::START), 0, row, 1, 1);
    api_key_entry_.set_visibility(false);
    grid->attach(api_key_entry_, 1, row, 1, 1);
    ++row;

    // Model — selector + entry side by side
    grid->attach(*Gtk::make_managed<Gtk::Label>("Model", Gtk::Align::START), 0, row, 1, 1);
    auto model_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    model_box->append(model_combo_);
    model_entry_.set_hexpand(true);
    model_entry_.set_placeholder_text("Or type a custom model…");
    model_box->append(model_entry_);
    refresh_models_btn_.set_label("⟳");
    refresh_models_btn_.set_tooltip_text("Refresh models from provider");
    model_box->append(refresh_models_btn_);
    grid->attach(*model_box, 1, row, 1, 1);
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
    refresh_models();

    // ── Signals ──────────────────────────────────────────────────────────────
    signal_response().connect(sigc::mem_fun(*this, &SettingsDialog::on_save));
    provider_combo_.signal_changed().connect(
        sigc::mem_fun(*this, &SettingsDialog::on_provider_changed));
    model_combo_.signal_changed().connect([this]() {
        std::string sel = model_combo_.get_active_text();
        if (!sel.empty())
            model_entry_.set_text(sel);
    });
    refresh_models_btn_.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsDialog::refresh_models));
}

void SettingsDialog::populate_from_config()
{
    const auto& cfg = Config::instance();

    // Populate provider combo from the providers table.
    populate_providers();

    // Select the current provider.
    int current_pid = cfg.provider_id();
    auto providers = Config::instance().list_providers();
    for (size_t i = 0; i < providers.size(); ++i) {
        if (providers[i].id == current_pid) {
            provider_combo_.set_active(i);
            break;
        }
    }

    // Show the current API key for this provider.
    api_key_entry_.set_text(cfg.provider_api_key());

    model_entry_.set_text(cfg.model());

    auto buf = prompt_textview_.get_buffer();
    buf->set_text(cfg.system_prompt_template());

    on_provider_changed();
}

void SettingsDialog::populate_providers()
{
    provider_combo_.remove_all();
    auto providers = Config::instance().list_providers();
    for (const auto& p : providers) {
        // Store the provider id as a data attribute on the combo row.
        auto id_str = std::to_string(p.id);
        provider_combo_.append(id_str, p.name);
    }
}

void SettingsDialog::refresh_models()
{
    int pid = selected_provider_id();
    auto models = Config::instance().list_models(pid);

    std::string current = model_entry_.get_text();

    while (model_combo_.get_model()->children().size() > 0)
        model_combo_.remove_text(0);

    for (const auto& m : models)
        model_combo_.append(m.name);

    if (pid == 1) {
        // Look up Ollama's URL — Config still has the old provider_id
        // until the user saves, so cfg.provider_url() would be wrong.
        std::string ollama_url = "http://localhost:11434";
        for (const auto& p : Config::instance().list_providers()) {
            if (p.id == 1) { ollama_url = p.base_url; break; }
        }
        auto live = OllamaClient::list_models(ollama_url);
        for (const auto& name : live)
            model_combo_.append(name);
    }

    // If the dropdown is still empty, add a placeholder so it stays interactive.
    if (model_combo_.get_model()->children().size() == 0)
        model_combo_.append("— no models found —");

    // Restore the entry text and try to select it in the dropdown.
    model_entry_.set_text(current);
    model_combo_.set_active_text(current);
}

int SettingsDialog::selected_provider_id() const
{
    auto active = provider_combo_.get_active_id();
    return active.empty() ? 1 : std::stoi(active);
}

void SettingsDialog::on_provider_changed()
{
    int pid = selected_provider_id();
    bool is_ollama = (pid == 1);

    api_key_entry_.set_sensitive(!is_ollama);
    refresh_models_btn_.set_sensitive(is_ollama);
    model_entry_.set_sensitive(true);
    model_combo_.set_sensitive(true);

    // Load the stored API key and last model for this provider.
    auto providers = Config::instance().list_providers();
    for (const auto& p : providers) {
        if (p.id == pid) {
            api_key_entry_.set_text(p.api_key);
            model_entry_.set_text(p.last_model);
            model_combo_.set_active_text(p.last_model);
            break;
        }
    }

    refresh_models();
}

void SettingsDialog::on_save(int response)
{
    if (response != Gtk::ResponseType::OK) return;

    auto& cfg = Config::instance();

    int pid = selected_provider_id();
    cfg.set_provider_id(pid);

    // Save the API key back to the provider record.
    SavedProvider provider;
    auto providers = Config::instance().list_providers();
    for (const auto& p : providers) {
        if (p.id == pid) {
            provider = p;
            break;
        }
    }

    // Read the model from the UI before saving.
    std::string model = model_entry_.get_text();
    if (model.empty() || model.find("— no models —") != std::string::npos) {
        auto active = model_combo_.get_active_text();
        if (!active.empty() && active.find("— no models —") == std::string::npos)
            model = active;
        else
            model.clear();
    }

    provider.api_key    = api_key_entry_.get_text();
    provider.last_model = model;
    Config::instance().save_provider(provider);

    cfg.set_system_prompt_template(prompt_textview_.get_buffer()->get_text());
    cfg.save();
}
