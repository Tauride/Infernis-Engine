// Entry points of editor features that are not built yet in the Infernis port. Each logs what is missing, so a
// menu command that reaches one says so instead of failing to link. They go away as the stages bring the
// features in (see code/editors/README.md):
// - skinned-mesh export (ExportSkeleton.cpp; needs MagicSoftware and DirectXMesh): Actor Editor stage;
// - the image editor (UIImageEditorForm.cpp; uses IX-Ray's RedImage): with the texture tools;
// - the light preview's accumulation geometry (EDITOR_LIGHT_PREVIEW): Level Editor stage.
#include "stdafx.h"
#include "ExportSkeleton.h"
#include "UIImageEditorForm.h"

ECORE_API float g_EpsSkelPositionDelta = EPS_L; // used by the static OGF export as well (IX-Ray value)

namespace
{
void NotPorted(const char* Feature)
{
    ELog.DlgMsg(mtError, "%s is not ported to Infernis yet.", Feature);
}
} // namespace

// Skinned-mesh export
u16 CSkeletonCollectorPacked::VPack(SSkelVert&)
{
    R_ASSERT2(false, "Skinned-mesh export is not ported to Infernis yet");
    return 0;
}

CExportSkeleton::CExportSkeleton(CEditableObject* object) : m_Source(object) {}
bool CExportSkeleton::Export(IWriter&, u8) { NotPorted("Skinned-mesh export"); return false; }
bool CExportSkeleton::ExportGeometry(IWriter&, u8) { NotPorted("Skinned-mesh export"); return false; }
bool CExportSkeleton::ExportMotions(IWriter&) { NotPorted("Motion export"); return false; }
bool CExportSkeleton::ExportMotionKeys(IWriter&) { NotPorted("Motion export"); return false; }
bool CExportSkeleton::ExportMotionDefs(IWriter&) { NotPorted("Motion export"); return false; }

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
