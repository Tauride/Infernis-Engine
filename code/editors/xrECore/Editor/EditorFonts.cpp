#include "stdafx.h"
#include "EditorFonts.h"

ECORE_API CFontManager* g_FontManager = nullptr;

CFontManager::~CFontManager()
{
    for (auto& [Key, Font] : Fonts)
        xr_delete(Font);
}

CGameFont* CFontManager::GetFont(const char* Section, u32 Flags)
{
    CGameFont*& Font = Fonts[std::make_pair(shared_str(Section), Flags)];
    if (!Font)
        Font = new CGameFont(Section, Flags);
    return Font;
}

void CFontManager::Render()
{
    for (auto& [Key, Font] : Fonts)
        Font->OnRender();
}
