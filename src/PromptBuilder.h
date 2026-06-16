#ifndef asktux_PROMPT_BUILDER_H
#define asktux_PROMPT_BUILDER_H

#include <string>
#include "SystemInfo.h"

/**
 * PromptBuilder — replaces placeholder tokens in a system-prompt template
 * with actual system info.
 *
 * Supported placeholders:
 *   {distro}, {desktop}, {desktop_version}, {window_system},
 *   {shell}, {hardware}, {user_question}
 */
class PromptBuilder {
public:
    /**
     * Build the final system prompt string.
     * @param template_str  The template containing placeholders.
     * @param info          The collected system information.
     * @param user_question The user's raw question.
     * @return              The template with placeholders replaced.
     */
    static std::string build(const std::string& template_str,
                             const SystemInfo& info,
                             const std::string& user_question);
};

#endif // asktux_PROMPT_BUILDER_H
