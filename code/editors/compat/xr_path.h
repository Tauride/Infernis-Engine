#pragma once
// IX-Ray's xr_path (std::filesystem::path that converts to and from xr_string), made header-only for the editors.
// Ported from IX-Ray xrCore/xr_path.h and xr_path.cpp; Infernis's xrCore has no equivalent.

#include <filesystem>

class CFilePath : public std::filesystem::path
{
    using inherited = std::filesystem::path;

    static xr_string to_xr_string(const inherited& p)
    {
        const std::wstring wide = p.wstring();
        if (wide.empty())
            return xr_string();
        const int size = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return xr_string();
        xr_string result(size, '\0');
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), result.data(), size, nullptr, nullptr);
        return result;
    }

public:
    CFilePath() = default;
    CFilePath(const xr_string& Str) : inherited(Str.c_str()) {}
    CFilePath(const char* Str) : inherited(Str) {}
    CFilePath(const std::filesystem::directory_entry& Entry) : inherited(Entry.path()) {}
    CFilePath(const std::filesystem::path& Path) : inherited(Path) {}

    [[nodiscard]] xr_string xstring() const { return to_xr_string(*this); }
    [[nodiscard]] xr_string xfilename() const { return to_xr_string(inherited::filename()); }

    // Callers must use xstring() / xfilename(), which return engine strings.
    [[nodiscard]] inherited string() const = delete;
    [[nodiscard]] inherited filename() const = delete;

    operator xr_string() const { return to_xr_string(*this); }

    CFilePath& operator=(const CFilePath& Right) = default;
    CFilePath& operator=(const xr_string& Right)
    {
        inherited::operator=(Right.c_str());
        return *this;
    }
    CFilePath& operator=(const char* Right)
    {
        inherited::operator=(Right);
        return *this;
    }

    friend CFilePath operator/(const CFilePath& Left, const CFilePath& Right)
    {
        CFilePath Path = Left;
        Path += "\\";
        Path += static_cast<const inherited&>(Right);
        return Path;
    }

    static bool exists(const CFilePath& Path) { return std::filesystem::exists(Path); }
};

using xr_path = CFilePath;
using xr_dir_entry = std::filesystem::directory_entry;
using xr_dir_iter = std::filesystem::directory_iterator;
using xr_dir_recursive_iter = std::filesystem::recursive_directory_iterator;
