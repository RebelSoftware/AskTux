linhelp_sources = files(
  'main.cpp',
  'MainWindow.cpp',
  'SystemInfo.cpp',
  'Config.cpp',
  'PromptBuilder.cpp',
  'LLMClient.cpp',
  'OllamaClient.cpp',
  'OpenAIClient.cpp',
  'StreamParser.cpp',
  'SettingsDialog.cpp',
  'MarkdownRenderer.cpp',
)

linhelp_headers = files(
  'MainWindow.h',
  'SystemInfo.h',
  'Config.h',
  'PromptBuilder.h',
  'LLMClient.h',
  'OllamaClient.h',
  'OpenAIClient.h',
  'StreamParser.h',
  'SettingsDialog.h',
  'MarkdownRenderer.h',
)

webkit_dep = dependency('webkitgtk-6.0', required : true)
cmark_dep  = dependency('libcmark',     required : true)

linhelp_exe = executable('linhelp',
  linhelp_sources,
  dependencies : [gtkmm_dep, curl_dep, nlohmann_json_dep, thread_dep,
                  webkit_dep, cmark_dep],
  install : true,
)
