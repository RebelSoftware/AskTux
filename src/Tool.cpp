#include "Tool.h"

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>

// ── Helper: run a shell command with a 5-second timeout ──────────────────────
static std::string run_cmd(const std::string& cmd)
{
    std::string result;
    std::array<char, 4096> buf;
    FILE* pipe = popen(("timeout 5 " + cmd + " 2>&1").c_str(), "r");
    if (!pipe) return "Error: failed to run command.";
    while (fgets(buf.data(), buf.size(), pipe) != nullptr)
        result += buf.data();
    int rc = pclose(pipe);
    // Trim trailing whitespace.
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    if (result.empty())
        result = "(no output)";
    return result + "\n(exit code " + std::to_string(rc) + ")";
}

// ── Individual tool implementations ──────────────────────────────────────────

static std::string tool_man_pages(const std::string& args)
{
    // args is a command name like "ls" or "grep"
    std::string cmd = args;
    // Strip leading/trailing quotes if present.
    if (cmd.size() >= 2 && cmd.front() == '"' && cmd.back() == '"')
        cmd = cmd.substr(1, cmd.size() - 2);

    if (cmd.empty())
        return "Error: no command specified. Usage: man_pages args=\"command\"";

    return run_cmd("man " + cmd + " 2>/dev/null | head -200");
}

static std::string tool_which(const std::string& args)
{
    std::string cmd = args;
    if (cmd.size() >= 2 && cmd.front() == '"' && cmd.back() == '"')
        cmd = cmd.substr(1, cmd.size() - 2);

    if (cmd.empty())
        return "Error: no command specified. Usage: which args=\"command\"";

    return run_cmd("which " + cmd);
}

static std::string tool_apt_search(const std::string& args)
{
    std::string query = args;
    if (query.size() >= 2 && query.front() == '"' && query.back() == '"')
        query = query.substr(1, query.size() - 2);

    if (query.empty())
        return "Error: no search query. Usage: apt_search args=\"search term\"";

    return run_cmd("apt-cache search " + query + " 2>/dev/null | head -30");
}

static std::string tool_find_apps(const std::string& args)
{
    std::string keyword = args;
    if (keyword.size() >= 2 && keyword.front() == '"' && keyword.back() == '"')
        keyword = keyword.substr(1, keyword.size() - 2);

    if (keyword.empty())
        return "Error: no keyword. Usage: find_apps args=\"keyword\"";

    // Search .desktop files for the keyword in Name or Categories.
    std::string result = run_cmd(
        "grep -ril " + keyword
        + " /usr/share/applications/ ~/.local/share/applications/ 2>/dev/null "
          "| head -20 | while read f; do echo \"$(grep -m1 '^Name=' \"$f\" "
          "| cut -d= -f2)  ($(grep -m1 '^Categories=' \"$f\" "
          "| cut -d= -f2))\"; done | sort -u");
    return result;
}

static std::string tool_check_installed(const std::string& args)
{
    std::string pkg = args;
    if (pkg.size() >= 2 && pkg.front() == '"' && pkg.back() == '"')
        pkg = pkg.substr(1, pkg.size() - 2);

    if (pkg.empty())
        return "Error: no package name. Usage: check_installed args=\"package\"";

    std::string result = run_cmd("dpkg -l " + pkg + " 2>/dev/null | tail -1");
    if (result.find("ii ") != std::string::npos)
        return pkg + " is INSTALLED.";
    return pkg + " is NOT installed.";
}

// ── Singleton ────────────────────────────────────────────────────────────────

ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry reg;
    static bool init = false;
    if (!init) {
        init = true;
        reg.register_tool(
            {"man_pages",
             "Read the manual page (man page) for a Linux command.",
             "man_pages args=\"command\""},
            tool_man_pages);

        reg.register_tool(
            {"which",
             "Check if a program is installed and where it is located.",
             "which args=\"program\""},
            tool_which);

        reg.register_tool(
            {"apt_search",
             "Search available packages in the APT repository.",
             "apt_search args=\"search terms\""},
            tool_apt_search);

        reg.register_tool(
            {"check_installed",
             "Check whether a specific Debian/Ubuntu package is installed.",
             "check_installed args=\"package-name\""},
            tool_check_installed);

        reg.register_tool(
            {"find_apps",
             "Search installed GUI applications by keyword (e.g. 'editor', 'browser', 'media')."
             " Use this to discover what software is already on the system.",
             "find_apps args=\"keyword\""},
            tool_find_apps);
    }
    return reg;
}

// ── Registration ─────────────────────────────────────────────────────────────

void ToolRegistry::register_tool(Tool tool, ToolExecutor exec)
{
    entries_.push_back({std::move(tool), std::move(exec)});
}

// ── Descriptions for the system prompt ───────────────────────────────────────

std::string ToolRegistry::tool_descriptions() const
{
    std::ostringstream out;
    out << "\nAvailable tools (call by including a line with [TOOL: ...] in your "
           "response):\n";
    for (const auto& e : entries_) {
        out << "- `[TOOL: " << e.tool.usage << "]` — " << e.tool.description
            << "\n";
    }
    out << "\n"
           "When you need to look something up, output ONLY the tool call line\n"
           "and NOTHING else (no introductions, no explanations). For example:\n"
           "\n"
           "[TOOL: find_apps args=\"editor\"]\n"
           "\n"
           "After the result comes back, you will be asked to continue your\n"
           "response normally.\n"
           "IMPORTANT: Only call ONE tool at a time. Wait for the result before\n"
           "calling another or continuing.\n";
    return out.str();
}

// ── Parse a tool call from text ──────────────────────────────────────────────

bool ToolRegistry::parse_tool_call(const std::string& text,
                                   std::string& name,
                                   std::string& args) const
{
    // Match [TOOL: name args="..."] or [TOOL: name]
    std::regex re(R"(\[TOOL:\s+(\w+)(?:\s+args=\"([^\"]*)\")?\])");
    std::smatch m;
    if (std::regex_search(text, m, re)) {
        name = m[1].str();
        args = m[2].matched ? m[2].str() : "";
        return true;
    }
    return false;
}

// ── Execution ────────────────────────────────────────────────────────────────

std::string ToolRegistry::execute(const std::string& name,
                                  const std::string& args) const
{
    for (const auto& e : entries_) {
        if (e.tool.name == name) {
            return e.exec(args);
        }
    }
    return "Error: unknown tool \"" + name + "\". Available tools: "
           + ([this]() -> std::string {
                  std::string names;
                  for (const auto& e : entries_) {
                      if (!names.empty()) names += ", ";
                      names += e.tool.name;
                  }
                  return names;
              })();
}

std::string ToolRegistry::execute_from_text(const std::string& text) const
{
    std::string name, args;
    if (!parse_tool_call(text, name, args))
        return {};
    return execute(name, args);
}
