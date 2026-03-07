// cli/terminal_ui.cpp
// Owner: Dev D
#include "terminal_ui.hpp"
#include <iostream>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace guardian {
namespace cli {

void TerminalUI::init() {
#ifdef _WIN32
    // Enable ANSI escape sequence processing on Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    // ENABLE_VIRTUAL_TERMINAL_PROCESSING == 0x0004
    dwMode |= 0x0004;
    SetConsoleMode(hOut, dwMode);
#endif
}

void TerminalUI::clear_screen() {
    std::cout << "\033[2J\033[H";
}

void TerminalUI::print_header(const std::string& title) {
    std::cout << "\n" << Color::Bold << Color::Cyan
              << "================================================================================\n"
              << "  " << title << "\n"
              << "================================================================================\n"
              << Color::Reset;
}

void TerminalUI::print_divider() {
    std::cout << "--------------------------------------------------------------------------------\n";
}

void TerminalUI::print_step(const std::string& step_name, const std::string& description) {
    std::cout << Color::Bold << "[" << step_name << "] " << Color::Reset << description << "\n";
}

void TerminalUI::print_success(const std::string& message) {
    std::cout << Color::Green << "âœ“ " << message << Color::Reset << "\n";
}

void TerminalUI::print_error(const std::string& message) {
    std::cout << Color::Red << "âœ— " << message << Color::Reset << "\n";
}

void TerminalUI::print_warning(const std::string& message) {
    std::cout << Color::Yellow << "⚠ " << message << Color::Reset << "\n";
}

void TerminalUI::wait_for_keypress() {
    std::cout << "\n" << Color::Bold << "Press Enter to continue..." << Color::Reset;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int TerminalUI::prompt_menu(const std::string& prompt, const std::vector<std::string>& options) {
    std::cout << "\n" << Color::Bold << prompt << Color::Reset << "\n";
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << "  " << Color::Cyan << (i + 1) << ". " << Color::Reset << options[i] << "\n";
    }
    
    int choice = 0;
    while (true) {
        std::cout << "\nSelect an option (1-" << options.size() << "): ";
        if (std::cin >> choice && choice >= 1 && choice <= static_cast<int>(options.size())) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return choice;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << Color::Red << "Invalid choice. Please try again." << Color::Reset << "\n";
    }
}

std::string TerminalUI::wrap_color(const std::string& text, const char* color) {
    return std::string(color) + text + Color::Reset;
}

} // namespace cli
} // namespace guardian
