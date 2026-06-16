#ifndef asktux_SETTINGS_DIALOG_H
#define asktux_SETTINGS_DIALOG_H

#include <gtkmm.h>
#include <string>

/**
 * SettingsDialog — modal dialog for editing asktux configuration.
 *
 * Fields: provider (combo from providers table), model, system prompt.
 * On Save, values are written to Config and the dialog closes.
 */
class SettingsDialog : public Gtk::Dialog {
public:
    SettingsDialog(Gtk::Window& parent);
    ~SettingsDialog() override = default;

private:
    void populate_from_config();
    void populate_providers();
    void on_save(int response);
    void on_provider_changed();
    void refresh_models();
    int  selected_provider_id() const;

    // ── Widgets ──────────────────────────────────────────────────────────────
    Gtk::ComboBoxText  provider_combo_;   // dropdown of saved providers
    Gtk::ComboBoxText  model_combo_;      // dropdown of models for this provider
    Gtk::Entry         model_entry_;      // type any model name
    Gtk::Button        refresh_models_btn_;
    Gtk::Entry         api_key_entry_;    // API key for the selected provider
    Gtk::TextView      prompt_textview_;
    Gtk::ScrolledWindow prompt_scroll_;
};

#endif // asktux_SETTINGS_DIALOG_H
