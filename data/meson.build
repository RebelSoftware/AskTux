# ── Desktop file ──────────────────────────────────────────────────────────────
install_data('asktux.desktop',
  install_dir : get_option('datadir') / 'applications',
)

# ── Icon ──────────────────────────────────────────────────────────────────────
install_data('icons/asktux.svg',
  install_dir : get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

# ── CSS stylesheet (editable post-compilation) ────────────────────────────────
install_data('style.css',
  install_dir : get_option('datadir') / 'asktux',
)

# ── Database schema (for inspection, manual setup, or install scripts) ────────
install_data('schema.sql',
  install_dir : get_option('datadir') / 'asktux',
)

# ── AppStream metadata (optional) ─────────────────────────────────────────────
# install_data('asktux.appdata.xml',
#   install_dir : get_option('datadir') / 'metainfo',
# )
