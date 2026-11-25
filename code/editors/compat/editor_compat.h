#pragma once
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
} // namespace Platform

using xr_atomic_bool = std::atomic_bool;

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
