project('asktux', 'cpp',
  version : '1.0.0',
  default_options : [
    'cpp_std=c++17',
    'buildtype=debugoptimized',
    'warning_level=3',
  ]
)

# ── Dependencies ──────────────────────────────────────────────────────────────
gtkmm_dep = dependency('gtkmm-4.0', required : true)
curl_dep  = dependency('libcurl',   required : true)
nlohmann_json_dep = dependency('nlohmann_json', required : true)
thread_dep = dependency('threads')
sqlite3_dep = dependency('sqlite3', required : true)

# ── Sub-directories ───────────────────────────────────────────────────────────
subdir('src')
subdir('data')

# ── Tests (optional) ──────────────────────────────────────────────────────────
if get_option('build_tests')
  subdir('tests')
endif
