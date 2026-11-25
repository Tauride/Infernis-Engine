// Entry points of editor features that are not built yet in the Infernis port. Each logs what is missing, so a
// menu command that reaches one says so instead of failing to link. They go away as the stages bring the
// features in (see code/editors/README.md):
// - the image editor (UIImageEditorForm.cpp; uses IX-Ray's RedImage): with the texture tools;
// - the light preview's accumulation geometry (EDITOR_LIGHT_PREVIEW): Level Editor stage.
#include "stdafx.h"
#include "UIImageEditorForm.h"


namespace
{
void NotPorted(const char* Feature)
{
    ELog.DlgMsg(mtError, "%s is not ported to Infernis yet.", Feature);
}
} // namespace

// Image editor
void UIImageEditorForm::Update() {}
void UIImageEditorForm::Show(bool) { NotPorted("The image editor"); }
void UIImageEditorForm::ImportTextures() { NotPorted("Texture import"); }
void UIImageEditorForm::FindInEditor(xr_string, bool) {}

// Light preview (EDITOR_LIGHT_PREVIEW)
void CRenderTarget::accum_point_geom_destroy() {}
void CRenderTarget::accum_spot_geom_destroy() {}

// The editor's ImGui pass message (IX-Ray's engine declares it; see InfernisDevice.h).
ECORE_API void __fastcall rp_DrawUI(void* p)
{
    static_cast<pureDrawUI*>(p)->OnDrawUI();
}
