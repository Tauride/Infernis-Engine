#pragma once

// Dear ImGui integration (code/3rd-party/imgui: Win32 backend here, D3D9 and D3D11
// backends in the renderers): in-game debug overlays drawn on top of the frame, after
// the HUD and UI. Ported from the X-Ray 1.0 (Shadow of Chornobyl) engine fork.
// Renderer backends: D3D9 (R1, R2), D3D10 (R3), D3D11 (R4).

// psDebugOverlay - which overlays are shown (console: rs_show_fps, off by default)
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
