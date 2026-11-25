#include "stdafx.h"
#include "EditorFonts.h"

ECORE_API CFontManager* g_FontManager = nullptr;

CFontManager::~CFontManager()
{
    for (auto& [Section, Font] : Fonts)
        xr_delete(Font);
}

CGameFont* CFontManager::GetFont(const char* Section, u32 Flags)
{
    CGameFont*& Font = Fonts[shared_str(Section)];
    if (!Font)
        Font = new CGameFont(Section, Flags);
    return Font;
}

void CFontManager::Render()
{
    for (auto& [Section, Font] : Fonts)
        Font->OnRender();
}
