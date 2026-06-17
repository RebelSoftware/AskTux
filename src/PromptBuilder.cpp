#include "PromptBuilder.h"
#include "Tool.h"

static void replace_all(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::string PromptBuilder::build(const std::string& template_str,
                                  const SystemInfo& info,
                                  const std::string& user_question)
{
    std::string result = template_str;

    replace_all(result, "{distro}",          info.distro);
    replace_all(result, "{desktop}",         info.desktop);
    replace_all(result, "{desktop_version}", info.desktop_version);
    replace_all(result, "{window_system}",   info.window_system);
    replace_all(result, "{shell}",           info.shell);
    replace_all(result, "{hardware}",        info.hardware);
    replace_all(result, "{user_question}",   user_question);
    replace_all(result, "{tool_descriptions}",
                ToolRegistry::instance().tool_descriptions());

    return result;
}
