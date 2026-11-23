#include "stdafx.h"
#include "xr_imgui.h"
#include "../xrRender/RenderDeviceRender.h"

ENGINE_API Flags32 psDebugOverlay = {dbgShowFPS};

// xrCore marks memset/memmove/strcmp deprecated (C4995); imgui.h uses them.
#pragma warning(push)
#pragma warning(disable:4995)
#include <imgui.h>
#include <imgui_impl_win32.h>
#pragma warning(pop)

namespace
{
	bool	g_ready		= false;

	// The frame rate the colors are measured against: the rs_fps_limit cap,
	// or twice the refresh rate without a cap
	float TargetFPS()
	{
		u32		fps		= FpsLimitRate(psFpsLimit);
		if (!fps)		fps = FpsLimitRate(flDoubleRefresh);
		return	float(fps);
	}

	// Green at the target T, yellow from T/2, red below (at a 60 cap: green
	// >= 60, yellow >= 30). A capped game runs a hair under its cap, so green
	// starts 2% below T to keep it from flickering yellow.
	ImVec4 FPSColor(float fps)
	{
		const float		r		= fps / TargetFPS();
		return	(r >= 0.98f) ? ImVec4(0.40f, 1.00f, 0.40f, 1.f)
			:	(r >= 0.50f) ? ImVec4(1.00f, 0.85f, 0.30f, 1.f)
			:	               ImVec4(1.00f, 0.35f, 0.35f, 1.f);
	}

	// FPS history for the graph: the last 30 seconds of real time in 0.25 s
	// buckets, each holding the average FPS over its quarter second. It is
	// recorded every frame, even while the overlay is hidden.
	class CFPSHistory
	{
	public:
		enum { BUCKETS = 120 };
		static const float	BUCKET_TIME;	// seconds

		CFPSHistory			() { Clear(); }

		void Clear			()
		{
			m_count			= 0;
			m_head			= 0;
			m_time			= 0.f;
			m_frames		= 0.f;
		}

		void Update			(float dt)
		{
			// Longer than this is a pause (window inactive, device lost), not
			// a frame: start a new bucket instead of filling the graph with ~0.
			if (dt <= 0.f || dt > 5.f)		{ m_time = 0.f; m_frames = 0.f; return; }

			m_time			+= dt;
			m_frames		+= 1.f;
			if (m_time < BUCKET_TIME)		return;

			// A long frame spans several buckets: each gets the same FPS so
			// the time axis stays true. The rest of the time carries over
			// with its share of the frames.
			const float	fps		= m_frames / m_time;
			int			n		= int(m_time / BUCKET_TIME);
			m_time				-= n * BUCKET_TIME;
			m_frames			= m_time * fps;
			if (n > BUCKETS)	n = BUCKETS;
			for (int i = 0; i < n; ++i)		Push(fps);
		}

		int		Count		() const		{ return m_count; }
		// i = 0 is the oldest sample
		float	Get			(int i) const	{ return m_fps[(m_head - m_count + i + BUCKETS) % BUCKETS]; }

	private:
		void Push			(float fps)
		{
			m_fps[m_head]	= fps;
			m_head			= (m_head + 1) % BUCKETS;
			if (m_count < BUCKETS)	++m_count;
		}

		float	m_fps[BUCKETS];
		int		m_count;
		int		m_head;			// where the next sample goes
		float	m_time;			// in the current bucket
		float	m_frames;		// in the current bucket
	};
	const float CFPSHistory::BUCKET_TIME = 0.25f;

	CFPSHistory		g_fps_history;

	// The graph: newest sample on the right, line colored like the FPS number
	// (green at the target T, yellow from T/2, red below). The scale starts at
	// 0 and its top is the highest sample rounded up to a multiple of T/2 (at
	// least T); faint guide lines mark T/2 and T.
	void DrawFPSGraph()
	{
		const ImVec2	size	(ImGui::GetFontSize() * 16.f, ImGui::GetFontSize() * 4.f);
		const ImVec2	p0		= ImGui::GetCursorScreenPos();
		const ImVec2	p1		(p0.x + size.x, p0.y + size.y);
		ImGui::Dummy	(size);

		ImDrawList*		dl		= ImGui::GetWindowDrawList();
		dl->AddRectFilled	(p0, p1, IM_COL32(0, 0, 0, 90));
		dl->AddRect			(p0, p1, IM_COL32(255, 255, 255, 40));

		const int		count	= g_fps_history.Count();
		float			peak	= 0.f;
		for (int i = 0; i < count; ++i)
			if (g_fps_history.Get(i) > peak)	peak = g_fps_history.Get(i);
		const float		target	= TargetFPS();
		const float		half	= target / 2.f;
		const float		top		= (peak > target) ? half * ceilf(peak / half) : target;

		const ImU32		guide	= IM_COL32(255, 255, 255, 45);
		const ImU32		label	= IM_COL32(255, 255, 255, 110);
		// The T label wins when the two would overlap (a top far above T)
		const float		marks[]	= { target, half };
		float			taken	= p0.y - ImGui::GetFontSize();	// bottom of the label above
		for (int m = 0; m < 2; ++m)
		{
			const float	y		= p1.y - size.y * marks[m] / top;
			dl->AddLine		(ImVec2(p0.x, y), ImVec2(p1.x, y), guide);
			// Just below the guide, but inside the graph
			const float	ly		= std::min(y + 1.f, p1.y - ImGui::GetFontSize());
			if (ly < taken)		continue;
			char		buf[8];
			xr_sprintf		(buf, "%d", int(marks[m]));
			dl->AddText		(ImVec2(p0.x + 2.f, ly), label, buf);
			taken			= ly + ImGui::GetFontSize();
		}
		if (top > target)
		{
			char		buf[8];
			xr_sprintf		(buf, "%d", int(top));
			dl->AddText		(ImVec2(p0.x + 2.f, p0.y), label, buf);
		}

		if (count < 2)		return;
		const float		step	= size.x / float(CFPSHistory::BUCKETS - 1);
		const float		x0		= p1.x - step * float(count - 1);	// right-aligned
		ImVec2			prev	(x0, p1.y - size.y * g_fps_history.Get(0) / top);
		for (int i = 1; i < count; ++i)
		{
			const float	fps		= g_fps_history.Get(i);
			const ImVec2 cur	(x0 + step * float(i), p1.y - size.y * fps / top);
			dl->AddLine		(prev, cur, ImGui::GetColorU32(FPSColor(fps)), 1.5f);
			prev			= cur;
		}
	}

	// FPS and frame time in the top right corner. Both come from io.Framerate,
	// ImGui's rolling average over the last 60 frames of real (wall clock)
	// time, so they are steady and unaffected by the game time factor.
	// Below them, the FPS graph for the last 30 seconds.
	void DrawFPSOverlay()
	{
		const ImGuiIO&			io	= ImGui::GetIO();
		const ImGuiViewport*	vp	= ImGui::GetMainViewport();
		const float				pad	= 10.f;

		ImGui::SetNextWindowPos		(ImVec2(vp->WorkPos.x + vp->WorkSize.x - pad, vp->WorkPos.y + pad), ImGuiCond_Always, ImVec2(1.f, 0.f));
		ImGui::SetNextWindowBgAlpha	(0.35f);

		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration		| ImGuiWindowFlags_NoInputs		|
			ImGuiWindowFlags_NoNav				| ImGuiWindowFlags_NoMove		|
			ImGuiWindowFlags_AlwaysAutoResize	| ImGuiWindowFlags_NoSavedSettings	|
			ImGuiWindowFlags_NoFocusOnAppearing;

		if (ImGui::Begin("##xr_fps_overlay", NULL, flags))
		{
			const float		fps		= io.Framerate;
			ImGui::TextColored	(FPSColor(fps), "FPS   %6.1f", fps);
			ImGui::Text			("Frame %6.2f ms", (fps > 0.f) ? 1000.f / fps : 0.f);
			DrawFPSGraph		();
		}
		ImGui::End();
	}
}

void xr_imgui::OnDeviceCreate()
{
	if (g_ready)				return;

	IMGUI_CHECKVERSION			();
	ImGui::CreateContext		();

	ImGuiIO& io					= ImGui::GetIO();
	io.IniFilename				= NULL;		// no imgui.ini next to the game
	io.LogFilename				= NULL;
	// The game owns the cursor; the overlays take no input yet.
	io.ConfigFlags				|= ImGuiConfigFlags_NoMouseCursorChange;

	ImGui::StyleColorsDark		();

	R_ASSERT					(ImGui_ImplWin32_Init(Device.m_hWnd));
	// The renderer's backend (D3D9 or D3D11). A renderer without one: no overlays.
	if (!Device.m_pRender->ImGuiCreate(ImGui::GetCurrentContext())) {
		ImGui_ImplWin32_Shutdown	();
		ImGui::DestroyContext		();
		Msg							("* ImGui: this renderer has no backend, the debug overlays are off");
		return;
	}
	g_ready						= true;
}

void xr_imgui::OnDeviceDestroy()
{
	if (!g_ready)				return;
	g_ready						= false;

	Device.m_pRender->ImGuiDestroy	();
	ImGui_ImplWin32_Shutdown	();
	ImGui::DestroyContext		();
}

void xr_imgui::OnDeviceResetBegin()
{
	if (g_ready)				Device.m_pRender->ImGuiResetBegin();
}

void xr_imgui::OnDeviceResetEnd()
{
	if (g_ready)				Device.m_pRender->ImGuiResetEnd();
}

void xr_imgui::Render()
{
	if (!g_ready)				return;

	Device.m_pRender->ImGuiNewFrame	();
	ImGui_ImplWin32_NewFrame	();
	// Draw in back buffer coordinates (the backends take the client rect).
	ImGui::GetIO().DisplaySize	= ImVec2(float(Device.dwWidth), float(Device.dwHeight));
	ImGui::NewFrame				();

	g_fps_history.Update		(ImGui::GetIO().DeltaTime);

	if (psDebugOverlay.test(dbgShowFPS))	DrawFPSOverlay();

	ImGui::Render				();
	ImDrawData* draw_data		= ImGui::GetDrawData();
	if (draw_data->CmdListsCount > 0)
		Device.m_pRender->ImGuiRender	(draw_data);
}

