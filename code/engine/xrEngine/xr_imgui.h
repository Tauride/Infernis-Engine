#pragma once

// Dear ImGui integration (code/3rd-party/imgui: Win32 backend here, D3D9 and D3D11
// backends in the renderers): in-game debug overlays drawn on top of the frame, after
// the HUD and UI. Ported from the X-Ray 1.0 (Shadow of Chornobyl) engine fork.
// The DX10 renderer (R3) has no backend: nothing is drawn there.

// psDebugOverlay - which overlays are shown (console: rs_show_fps)
enum {
	dbgShowFPS						= (1ul<<0ul),
};
ENGINE_API extern	Flags32		psDebugOverlay;

namespace xr_imgui
{
	void	OnDeviceCreate		();		// after HW.CreateDevice
	void	OnDeviceDestroy		();		// before HW.DestroyDevice
	void	OnDeviceResetBegin	();		// before HW.Reset
	void	OnDeviceResetEnd	();		// after HW.Reset
	void	Render				();		// between BeginScene and EndScene
};
