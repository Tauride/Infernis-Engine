#pragma once
// A small font manager for the editors. IX-Ray's engine has a CFontManager (g_FontManager) that the editor core
// uses for stats and draw-utility text; Infernis's engine creates CGameFont objects directly from font sections
// (configs/fonts.ltx). This keeps IX-Ray's calls: GetFont by section, Render once per frame.

#include "xrEngine/GameFont.h"

class ECORE_API CFontManager
{
public:
    ~CFontManager();

    void InitializeFonts() {}
    CGameFont* GetFont(const char* Section, u32 Flags = 0);
    void Render();

private:
    // One font per section and flags: the same section created with other flags is another font.
    xr_map<std::pair<shared_str, u32>, CGameFont*> Fonts;
};

extern ECORE_API CFontManager* g_FontManager;
