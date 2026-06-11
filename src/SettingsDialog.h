#ifndef LINHELP_SETTINGS_DIALOG_H
#define LINHELP_SETTINGS_DIALOG_H

#include <gtkmm.h>
#include <string>

/**
 * SettingsDialog — modal dialog for editing LinHelp configuration.
 *
 * Fields: backend, model (editable combo, auto-populated from Ollama),
 * Ollama URL, OpenAI URL, API key, system prompt.
 * On Save, values are written to Config and the dialog closes.
 */
class SettingsDialog : public Gtk::Dialog {
public:
    SettingsDialog(Gtk::Window& parent);
    ~SettingsDialog() override = default;

private:
    void populate_from_config();
    void populate_ollama_models();
    void on_save(int response);
    void on_backend_changed();

    // ── Widgets ──────────────────────────────────────────────────────────────
    Gtk::ComboBoxText  backend_combo_;
    Gtk::ComboBoxText  model_combo_;       // dropdown of installed models
    Gtk::Entry         model_entry_;       // type any model name here
    Gtk::Button        refresh_models_btn_;
    Gtk::Entry         ollama_url_entry_;
    Gtk::Entry         openai_url_entry_;
    Gtk::Entry         api_key_entry_;
    Gtk::TextView      prompt_textview_;
    Gtk::ScrolledWindow prompt_scroll_;
};

#endif // LINHELP_SETTINGS_DIALOG_H
