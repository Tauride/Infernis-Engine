#pragma once

#include "xrEngine/device.h"
#include "UI_Camera.h"
#include "xrRenderCommon/HWCaps.h"
#include "xrRenderCommon/HW.h"
#include "xrEngine/pure.h"
#include "xrCore/FTimer.h"
#include "EStats.h"
#include "xrEngine/Shader_xrLC.h"
#include "xrRenderCommon/Shader.h"
#include "xrRenderCommon/R_Backend.h"

//---------------------------------------------------------------------------
// refs
class CGameFont;
class CInifile;
class CResourceManager;
#undef CreateWindow
//------------------------------------------------------------------------------
class ECORE_API CEditorRenderDevice;
extern ECORE_API CEditorRenderDevice* EDevice;

// Infernis: the D3D9 objects live in the renderer's HW (xrRenderCommon/HW.h), not behind IX-Ray's RHI accessors.
#define REContext (HW.pDevice)
#define REDevice (HW.pDevice)
#define RETarget (HW.pBaseRT)
#define REDepth (HW.pBaseZB)

class ECORE_API CEditorRenderDevice :
	public CRenderDevice
{
	friend class CUI_Camera;
	friend class TUI;

private:
	float m_fNearer;

	ref_shader m_CurrentShader;
	ref_texture SearchIcon;

	void _SetupStates();
	void _Create(IReader* F);
	void _Destroy(bool bKeepTextures);

public:
	ref_shader m_WireShader;
	ref_shader m_SelectionShader;

	ref_texture texture_null;
	Fmaterial m_DefaultMat;

	float RenderRadius;
	u32 dwRealWidth, dwRealHeight;
	float m_ScreenQuality;

	u32 dwFillMode;
	u32 dwShadeMode;

	RECT NormalWinSize;
	bool NormalWinSizeSaved = false;
	bool isZoomed = false;

public:
	CEditorRenderDevice();
	virtual ~CEditorRenderDevice();


	virtual bool Paused() const { return false; };
	void time_factor(float);
	bool Create();
	void Destroy();
	void Resize(int w, int h, bool maximized);
	void ReloadTextures();
	void UnloadTextures();

	void RenderNearer(float f_Near);
	void ResetNearer();
	bool Begin();
	void End();

	void Initialize(void);
	void ShutDown(void);
	void Reset(IReader* F, bool bKeepTextures);

	void MaximizedWindow();
	void ResoreWindow(bool moving);
	void InitWindowStyle();

	virtual void DumpResourcesMemoryUsage()
	{
	}

	// Sprite rendering
	IC float _x2real(float x)
	{
		return (x + 1) * dwWidth * 0.5f;
	}

	IC float _y2real(float y)
	{
		return (y + 1) * dwHeight * 0.5f;
	}

	// draw
	void SetShader(ref_shader sh) { m_CurrentShader = sh; }
	ref_shader GetShader() { return m_CurrentShader; }
	void DP(ERHI_PRIMITIVE_TOPOLOGY pt, ref_geom geom, u32 startV, u32 pc);
	void DIP(ERHI_PRIMITIVE_TOPOLOGY pt, ref_geom geom, u32 baseV, u32 startV, u32 countV, u32 startI, u32 PC);

	IC void SetRS(D3DRENDERSTATETYPE p1, u32 p2)
	{
		VERIFY(b_is_Ready);
		CHK_DX(REDevice->SetRenderState(p1,p2));
	}

	IC void SetSS(u32 sampler, D3DSAMPLERSTATETYPE type, u32 value)
	{
		VERIFY(b_is_Ready);
		CHK_DX(REDevice->SetSamplerState(sampler,type,value));
	}

	// light&material
	IC void LightEnable(u32 dwLightIndex, bool bEnable)
	{
		CHK_DX(REDevice->LightEnable(dwLightIndex, bEnable));
	}

	IC void SetLight(u32 dwLightIndex, Flight& lpLight)
	{
		CHK_DX(REDevice->SetLight(dwLightIndex, (D3DLIGHT9*)&lpLight));
	}

	IC void SetMaterial(Fmaterial& mat)
	{
		CHK_DX(REDevice->SetMaterial((D3DMATERIAL9*)&mat));
	}

	IC void ResetMaterial()
	{
		CHK_DX(REDevice->SetMaterial((D3DMATERIAL9*)&m_DefaultMat));
	}

	// update
	void UpdateView();
	void FrameMove();

	bool MakeScreenshot(U32Vec& pixels, u32 width, u32 height);


	void InitTimer();

	// Mode control
	// Infernis: Pause and PreCache are not virtual in Infernis's CRenderDevice; these hide them.
	void Pause(bool bOn, bool bTimer, bool bSound, const char* reason)
	{
	}

	void PreCache(u32 amount, bool b_draw_loadscreen, bool b_wait_user_input)
	{
	}

	virtual void Clear();

public:
	Shader_xrLC_LIB ShaderXRLC;

	// camera
	CRegistrator<pureDrawUI> seqDrawUI;

	// Dependent classes
	CResourceManager* Resources;

private:
	virtual void AddSeqFrame(pureFrame* f, bool mt) override;
	virtual void RemoveSeqFrame(pureFrame* f) override;

public:
	HWND GetHWND() const;
	void CreateWindow();
	void DestryWindow();
	virtual void Reset(bool precache);
	bool IsEditorMode() { return true; } // Infernis: not in CRenderDevice
};

// video
enum
{
	rsFilterLinear = (1ul << 20ul),
	rsEdgedFaces = (1ul << 21ul),
	rsRenderTextures = (1ul << 22ul),
	rsFog = (1ul << 24ul),
	rsRenderRealTime = (1ul << 25ul),
	rsDrawGrid = (1ul << 26ul),
	rsDrawSafeRect = (1ul << 27ul),
	rsMuteSounds = (1ul << 28ul),
	rsEnvironment = (1ul << 29ul),
	rsDrawAxis = (1ul << 30ul),
	rsDisableAxisCube = (1ul << 31ul),
};

#define DEFAULT_CLEARCOLOR 0x00555555

#define REQ_CREATE()	if (!EDevice->bReady)	return;
#define REQ_DESTROY()	if (EDevice->bReady)	return;

#include "xrRenderCommon/R_Backend_Runtime.h"
