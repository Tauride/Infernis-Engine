#pragma once
#include <ppl.h> // xr_task_group
// Bridges between the IX-Ray editor sources and Infernis's xrCore. Included by every editor library's stdafx.h
// right after xrCore/xrCore.h. Keep additions here small and documented; anything engine code needs as well
// belongs in xrCore instead.

#include "xr_path.h"
#include "xr_delegate.h"

struct SDL_Window;

#ifdef XREUI_EXPORTS
#define XREUI_APPINFO_API __declspec(dllexport)
#else
#define XREUI_APPINFO_API __declspec(dllimport)
#endif

// IX-Ray keeps the main window in xrCore (g_AppInfo). Infernis's xrCore has no SDL, so the editors own it in xrEUI.
class CEditorAppInfo
{
public:
    SDL_Window* Window = nullptr;
};

extern XREUI_APPINFO_API CEditorAppInfo g_AppInfo;

// IX-Ray platform switches; the Infernis editors are Windows only.
#ifndef IXR_WINDOWS
#define IXR_WINDOWS 1
#endif

// IX-Ray's xrCore text conversions (ANSI is the system code page, which the engine uses for paths and text).
namespace Platform
{
inline xr_string ConvertCodePage(const xr_string& Text, UINT From, UINT To)
{
    if (Text.empty())
        return Text;
    const int WideSize = MultiByteToWideChar(From, 0, Text.c_str(), (int)Text.size(), nullptr, 0);
    if (WideSize <= 0)
        return xr_string();
    std::wstring Wide(WideSize, L'\0');
    MultiByteToWideChar(From, 0, Text.c_str(), (int)Text.size(), Wide.data(), WideSize);
    const int Size = WideCharToMultiByte(To, 0, Wide.c_str(), WideSize, nullptr, 0, nullptr, nullptr);
    if (Size <= 0)
        return xr_string();
    xr_string Result(Size, '\0');
    WideCharToMultiByte(To, 0, Wide.c_str(), WideSize, Result.data(), Size, nullptr, nullptr);
    return Result;
}

inline xr_string ANSI_TO_UTF8(const xr_string& Ansi) { return ConvertCodePage(Ansi, CP_ACP, CP_UTF8); }
inline xr_string UTF8_to_CP1251(const xr_string& Utf8) { return ConvertCodePage(Utf8, CP_UTF8, 1251); }
inline xr_string WideToCP1251(const wchar_t* Wide)
{
    const int Size = WideCharToMultiByte(1251, 0, Wide, -1, nullptr, 0, nullptr, nullptr);
    if (Size <= 1)
        return xr_string();
    xr_string Result(Size - 1, '\0');
    WideCharToMultiByte(1251, 0, Wide, -1, Result.data(), Size, nullptr, nullptr);
    return Result;
}
} // namespace Platform

using xr_atomic_bool = std::atomic_bool;
using xr_atomic_u32 = std::atomic<u32>;
template <typename T> using xr_shared_ptr = std::shared_ptr<T>;
using RTokenVecIt = RTokenVec::iterator;
using xr_task_group = concurrency::task_group; // IX-Ray: the same on Windows (xrCore/_thread_types.h)

// Borland calling-convention marker, empty in IX-Ray xrCore; GSC-era editor declarations still carry it.
#ifndef _BCL
#define _BCL
#endif
using U8Vec = xr_vector<u8>; // IX-Ray xrCore; Infernis has U16Vec and U32Vec only
using Fvector2Vec = xr_vector<Fvector2>; // IX-Ray xrCore
// IX-Ray xrCore hashes shared_str for unordered containers. Equal shared_str share one interned value, so the
// pointer is a valid hash (operator== compares the same pointer).
template <>
struct std::hash<shared_str>
{
    size_t operator()(const shared_str& Value) const noexcept { return std::hash<const void*>()(Value._get()); }
};

// IX-Ray xrCore helpers.
using FS_FileSetIt = FS_FileSet::iterator;

inline float _sqrt(float Value) { return std::sqrt(Value); }
template <class T>
constexpr T _sqr(T Value) { return Value * Value; } // GSC/IX-Ray xrCore

// Concatenates into a fixed buffer and returns it (IX-Ray xrCore's variadic xr_strconcat).
template <size_t Size, class... Parts>
char* xr_strconcat(char (&Destination)[Size], const Parts&... Text)
{
    Destination[0] = 0;
    (xr_strcat(Destination, Size, Text), ...);
    return Destination;
}

// IX-Ray profiler markers (Tracy); no profiler in the Infernis editors.
#ifndef PROF_EVENT
#define PROF_EVENT(...)
#define PROF_THREAD(...)
#define PROF_FRAME(...)
#define PROF_EVENT_DYNAMIC(...)
#endif

// IX-Ray's static xr_string::ToString.
inline xr_string xr_string_ToString(int Value) { return xr_string(std::to_string(Value).c_str()); }
inline xr_string xr_string_ToString(unsigned int Value) { return xr_string(std::to_string(Value).c_str()); }
inline xr_string xr_string_ToString(float Value) { return xr_string(std::to_string(Value).c_str()); }
inline xr_string xr_string_ToString(double Value) { return xr_string(std::to_string(Value).c_str()); }

// IX-Ray xrCore smart pointer helpers.
template <class T>
using xr_unique_ptr = std::unique_ptr<T>;
template <class T, class... Args>
xr_unique_ptr<T> xr_make_unique(Args&&... Arguments)
{
    return std::make_unique<T>(std::forward<Args>(Arguments)...);
}

// IX-Ray xrCore: make sure the folders of a file path exist.
inline void VerifyPath(const char* Path)
{
    std::error_code Error;
    std::filesystem::create_directories(std::filesystem::path(Path).parent_path(), Error);
}

// IX-Ray xrCore: file time to time_t.
inline time_t xr_chrono_to_time_t(std::filesystem::file_time_type Time)
{
    using namespace std::chrono;
    return system_clock::to_time_t(clock_cast<system_clock>(Time));
}
inline time_t xr_chrono_to_time_t(std::chrono::system_clock::time_point Time)
{
    return std::chrono::system_clock::to_time_t(Time);
}

// IX-Ray CEnvironment::SplitTime: seconds of the day to hours, minutes and seconds.
inline void EditorSplitTime(float Time, u32& Hours, u32& Minutes, u32& Seconds)
{
    const u32 Total = u32(Time);
    Hours = Total / 3600;
    Minutes = (Total % 3600) / 60;
    Seconds = Total % 60;
}

// Infernis's bones name themselves with std::string, IX-Ray's editor code compares them with shared_str.
inline bool operator==(const std::string& Left, const shared_str& Right) { return Left == (Right.c_str() ? Right.c_str() : ""); }
inline bool operator==(const shared_str& Left, const std::string& Right) { return Right == Left; }
inline bool operator!=(const std::string& Left, const shared_str& Right) { return !(Left == Right); }
inline bool operator!=(const shared_str& Left, const std::string& Right) { return !(Right == Left); }
// Exact overloads for text literals, so the two above do not make `shared_str == "text"` ambiguous.
inline bool operator==(const shared_str& Left, const char* Right) { return 0 == xr_strcmp(Left.c_str() ? Left.c_str() : "", Right ? Right : ""); }
inline bool operator==(const char* Left, const shared_str& Right) { return Right == Left; }
inline bool operator!=(const shared_str& Left, const char* Right) { return !(Left == Right); }
inline bool operator!=(const char* Left, const shared_str& Right) { return !(Right == Left); }

// More IX-Ray xrCore container aliases the editor code uses.
template <class T, int dim>
using FixedVector = svector<T, dim>; // IX-Ray xrCore/Containers/FixedVector.h; Infernis's equivalent is svector
using PlaneVec = xr_vector<Fplane>;
using PlaneIt = PlaneVec::iterator;
using boolVec = xr_vector<bool>;
using boolIt = boolVec::iterator;
using U8It = U8Vec::iterator;
using U16It = U16Vec::iterator;
using U32It = U32Vec::iterator;
using IntIt = IntVec::iterator;
using FvectorIt = FvectorVec::iterator;
using SStringVecIt = SStringVec::iterator;

// IX-Ray xrCore's critical section and its scope guard (Infernis's xrCore uses std::mutex directly).
class xrCriticalSection
{
public:
    void Enter() { Mutex.lock(); }
    void Leave() { Mutex.unlock(); }
    bool TryEnter() { return Mutex.try_lock(); }

private:
    std::recursive_mutex Mutex;
};

class xrCriticalSectionGuard
{
public:
    explicit xrCriticalSectionGuard(xrCriticalSection& Section) : Section(Section) { Section.Enter(); }
    explicit xrCriticalSectionGuard(xrCriticalSection* Section) : Section(*Section) { this->Section.Enter(); }
    ~xrCriticalSectionGuard() { Section.Leave(); }
    xrCriticalSectionGuard(const xrCriticalSectionGuard&) = delete;
    xrCriticalSectionGuard& operator=(const xrCriticalSectionGuard&) = delete;

private:
    xrCriticalSection& Section;
};

// Old xrCore macro, still used by engine headers the editors include (xrEngine/Shader_xrLC.h, GameMtlLib.h) but
// no longer defined anywhere in Infernis.
#ifndef DEFINE_VECTOR
#define DEFINE_VECTOR(type, vec, it) \
    using vec = xr_vector<type>;     \
    using it = vec::iterator
#endif

// IX-Ray's _flags compares with ==; the property grid compares Flags8/16/32 values that way.
template <class T>
bool operator==(const _flags<T>& Left, const _flags<T>& Right)
{
    return Left.flags == Right.flags;
}

// IX-Ray's RStringVec holds shared_str; Infernis's holds std::string. Editor code that means the IX-Ray type uses
// SharedStrVec.
using SharedStrVec = xr_vector<shared_str>;
using SharedStrVecIt = SharedStrVec::iterator;

inline shared_str _ListToSequence(const SharedStrVec& List)
{
    xr_string Result;
    for (const shared_str& Item : List)
    {
        if (!Result.empty())
            Result += ',';
        Result += Item.c_str() ? Item.c_str() : "";
    }
    return shared_str(Result.c_str());
}

// IX-Ray's xr_string is a class with extra members; Infernis's is a plain std::basic_string alias, so the editor
// code calls these free functions instead (same behaviour as IX-Ray's xrCore/xrstring.cpp).
inline bool xr_string_contains(const xr_string& Str, const xr_string& SubStr) { return Str.find(SubStr) != xr_string::npos; }

// IX-Ray's CLocatorAPI::fix_path: a path the file system does not know, under $fs_root$, becomes relative to it;
// lowercased either way.
inline xr_string EditorFixPath(const xr_string& File)
{
    xr_string Path = File;
    if (!FS.exist(File.c_str()))
    {
        const xr_string Root = FS.get_path("$fs_root$")->m_Path;
        if (xr_string_contains(Path, Root))
            Path = Path.substr(Root.length());
    }
    xr_strlwr(Path.data());
    return Path;
}

inline xr_vector<xr_string> xr_string_split(const xr_string& Str, char Separator = ',')
{
    xr_vector<xr_string> Result;
    size_t Begin = 0;
    for (size_t Cursor = 0; Cursor <= Str.size(); ++Cursor)
    {
        if (Cursor == Str.size() || Str[Cursor] == Separator)
        {
            if (Cursor > Begin)
                Result.emplace_back(Str, Begin, Cursor - Begin);
            Begin = Cursor + 1;
        }
    }
    return Result;
}

inline xr_string xr_string_remove_whitespaces(const xr_string& Str)
{
    xr_string Result;
    Result.reserve(Str.size());
    for (char Ch : Str)
        if (Ch != ' ')
            Result.push_back(Ch);
    return Result;
}

// IX-Ray xrCore/xrstring.h: true when the text is valid UTF-8 (plain ASCII and null included).
inline bool IsUTF8(const char* String)
{
    if (!String)
        return true;
    const unsigned char* Bytes = (const unsigned char*)String;
    while (*Bytes)
    {
        int Extra;
        if (*Bytes < 0x80)
            Extra = 0;
        else if ((*Bytes & 0xE0) == 0xC0)
            Extra = 1;
        else if ((*Bytes & 0xF0) == 0xE0)
            Extra = 2;
        else if ((*Bytes & 0xF8) == 0xF0)
            Extra = 3;
        else
            return false;
        ++Bytes;
        for (int i = 0; i < Extra; ++i, ++Bytes)
            if ((*Bytes & 0xC0) != 0x80)
                return false;
    }
    return true;
}

// FS path alias for gamedata\fonts. IX-Ray names it in LocatorAPI_defs.h; Infernis's fsgame.ltx has the entry
// but no constant.
#define _game_fonts_ "$game_fonts$"
#ifndef _game_textures_
#define _game_textures_ "$game_textures$"
#endif

// IX-Ray xrCore concepts used by editor templates.
namespace XRay::Concepts
{
template <class T>
concept Enum = std::is_enum_v<T>;
}

// IX-Ray RHI subset on Direct3D 9 (textures, cull mode, clears) shared by all editor modules.
#include "editor_rhi.h"
