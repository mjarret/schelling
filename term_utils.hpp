// term_utils.hpp — terminal width + compact formatting + SIGWINCH support (POSIX)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstddef>
#include <cstdlib>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <signal.h>
#endif

inline int get_terminal_width() {
#if defined(_WIN32)
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(h, &info)) return 80;
    return (int)(info.srWindow.Right - info.srWindow.Left + 1);
#else
    if (const char* env = std::getenv("COLUMNS")) {
        int c = std::atoi(env); if (c > 0) return c;
    }
    winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return (int)w.ws_col;
    return 80;
#endif
}

inline std::string trim_fit(const std::string& s, std::size_t max_len) {
    if (s.size() <= max_len) return s;
    if (max_len == 0) return {};
    if (max_len <= 3) return std::string(max_len, '.');
    return s.substr(0, max_len - 1) + "…";
}

inline std::string fmt_sci_short(double x, int sig=2) {
    if (!std::isfinite(x)) return "nan";
    std::ostringstream os; os.setf(std::ios::scientific); os << std::setprecision(sig) << x;
    std::string s = os.str(); // e.g. 1.23e-04
    auto epos = s.find('e');
    if (epos != std::string::npos && epos + 2 < s.size()) {
        char sign = s[epos+1];
        std::string exp = s.substr(epos+2);
        std::size_t i = 0; while (i + 1 < exp.size() && exp[i] == '0') ++i;
        exp = exp.substr(i);
        s = s.substr(0, epos+1) + (sign=='+'? "" : "-") + exp;
    }
    return s;
}

#if !defined(_WIN32)
#include <atomic>
inline std::atomic<bool>& terminal_resized_flag() { static std::atomic<bool> f{false}; return f; }
inline void term_utils_install_sigwinch() {
    static bool installed = false; if (installed) return; installed = true;
    struct sigaction sa{}; sa.sa_handler = [](int){ terminal_resized_flag().store(true, std::memory_order_relaxed); };
    sigemptyset(&sa.sa_mask); sa.sa_flags = SA_RESTART; sigaction(SIGWINCH, &sa, nullptr);
}
#else
inline void term_utils_install_sigwinch() {}
inline bool& dummy_win_flag() { static bool b=false; return b; }
#define terminal_resized_flag dummy_win_flag
#endif
