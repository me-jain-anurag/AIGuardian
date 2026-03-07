// cli/terminal_ui.hpp
// Owner: Dev D
// ANSI terminal styling, layout helpers, and Windows console setup
#pragma once

#include <string>
#include <vector>

namespace guardian {
namespace cli {

// Cross-platform ANSI color codes
struct Color {
    static constexpr const char* Reset   = "\033[0m";
    static constexpr const char* Bold    = "\033[1m";
    static constexpr const char* Red     = "\033[31m";
    static constexpr const char* Green   = "\033[32m";
    static constexpr const char* Yellow  = "\033[33m";
    static constexpr const char* Blue    = "\033[34m";
    static constexpr const char* Magenta = "\033[35m";
    static constexpr const char* Cyan    = "\033[36m";
    static constexpr const char* White   = "\033[37m";
};

class TerminalUI {
public:
    // Enables ENABLE_VIRTUAL_TERMINAL_PROCESSING on Windows. Does nothing on POSIX.
    static void init();

    // Layout
    static void clear_screen();
    static void print_header(const std::string& title);
    static void print_divider();

    // Styled printing
    static void print_step(const std::string& step_name, const std::string& description);
    static void print_success(const std::string& message);
    static void print_error(const std::string& message);
    static void print_warning(const std::string& message);

    // Interactive
    static void wait_for_keypress();
    static int prompt_menu(const std::string& prompt, const std::vector<std::string>& options);
    
    // Formatting helpers
    static std::string wrap_color(const std::string& text, const char* color);
};

} // namespace cli
} // namespace guardian
