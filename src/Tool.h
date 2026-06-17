#ifndef ASKTUX_TOOL_H
#define ASKTUX_TOOL_H

#include <functional>
#include <string>
#include <vector>

/**
 * A single tool that the LLM can invoke mid-response.
 *
 * The AI outputs a line like:
 *   [TOOL: man_pages args="ls"]
 *
 * The application catches it post-stream, executes the tool, and makes
 * a second streaming request with the result injected as context.
 */
struct Tool {
    std::string name;         // e.g. "man_pages"
    std::string description;  // what it does (shown in the system prompt)
    std::string usage;        // how the AI should call it, e.g. 'man_pages args="ls"'
};

using ToolExecutor = std::function<std::string(const std::string& args)>;

/**
 * ToolRegistry — singleton that holds the available tools and can
 * detect / execute tool calls.
 */
class ToolRegistry {
public:
    static ToolRegistry& instance();

    void register_tool(Tool tool, ToolExecutor exec);

    /** Return a markdown block describing all available tools (for the prompt). */
    std::string tool_descriptions() const;

    /**
     * Check if a string contains a tool call marker.
     * If found, returns true and fills name/args.
     */
    bool parse_tool_call(const std::string& text,
                         std::string& name,
                         std::string& args) const;

    /**
     * Execute a tool by name with the given args.
     * Returns the result string (or an error message).
     */
    std::string execute(const std::string& name,
                        const std::string& args) const;

    /** Convenience: find + extract the tool call and return the result. */
    std::string execute_from_text(const std::string& text) const;

private:
    ToolRegistry() = default;

    struct Entry {
        Tool          tool;
        ToolExecutor  exec;
    };
    std::vector<Entry> entries_;
};

#endif // ASKTUX_TOOL_H
