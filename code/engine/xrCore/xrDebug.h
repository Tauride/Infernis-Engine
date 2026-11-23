#pragma once

typedef void crashhandler();
typedef void on_dialog(bool before);
// Unattended mode: called once, right before the process terminates on a fatal error.
// reason - the assertion text or the exception description; dump_path - the minidump ("" if none).
typedef void on_unattended_crash(const char* reason, const char* dump_path);

class XRCORE_API xrDebug {
    crashhandler* handler = nullptr;
    on_dialog* m_on_dialog = nullptr;
    on_unattended_crash* m_on_unattended_crash = nullptr;

public:
    void _initialize();
    void _destroy();

    crashhandler* get_crashhandler() const { return handler; };
    void set_crashhandler(crashhandler* handler) { this->handler = handler; };

    on_dialog* get_on_dialog() const { return m_on_dialog; }
    void set_on_dialog(on_dialog* on_dialog) { m_on_dialog = on_dialog; }

    // Unattended mode (-unattended, implied by -autotest): fatal errors never show a dialog.
    // They log, write a minidump to $logs$, call the on_unattended_crash callback and
    // terminate the process with exit code 3.
    static bool unattended();
    on_unattended_crash* get_on_unattended_crash() const { return m_on_unattended_crash; }
    void set_on_unattended_crash(on_unattended_crash* cb) { m_on_unattended_crash = cb; }

    const char* error2string(const DWORD code) const;

    void gather_info(const std::string_view expression, const std::string_view description,
                     const std::string_view file, const int line, const std::string_view function,
                     char* assertion_info, const size_t assertion_info_size,
                     std::initializer_list<const std::string_view> args);

    template <size_t count>
    void gather_info(const std::string_view expression, const std::string_view description,
                     const std::string_view file, const int line, const std::string_view function,
                     char (&assertion_info)[count], std::initializer_list<const std::string_view> args) {
        gather_info(expression, description, file, line, function,
                    assertion_info, count, args);
    }

    void fail(const std::string_view expr, const std::string_view file, const int line,
              const std::string_view function, bool& ignore_always);
    void fail(const std::string_view reason, const std::string_view expr, const std::string_view file,
              const int line, const std::string_view function, bool& ignore_always);
    void fail(const std::string_view reason, const std::string_view expr,
              std::initializer_list<const std::string_view> args, const std::string_view file,
              const int line, const std::string_view function, bool& ignore_always);
    void error(const DWORD code, const std::string_view expr, std::initializer_list<const std::string_view> args,
               const std::string_view file, const int line, const std::string_view function, bool& ignore_always);

    template <typename... Args>
    void fatal(const std::string_view file, const int line, const std::string_view function,
               const char* format, const Args&... args) {
        std::array<char, 2048> buffer;
        std::snprintf(buffer.data(), buffer.size(), format, args...);
        bool ignore_always = true;

        backend("fatal error", "<no expression>", { buffer.data() }, file, line, function, ignore_always);
    }

    void backend(const std::string_view reason, const std::string_view expression,
                 std::initializer_list<const std::string_view> args,
                 const std::string_view file, const int line, const std::string_view function,
                 bool& ignore_always);
    [[noreturn]] static void do_exit(const std::string& message);
};

extern XRCORE_API xrDebug Debug;

XRCORE_API void LogStackTrace(const char* header);

#include "xrDebug_macros.h"