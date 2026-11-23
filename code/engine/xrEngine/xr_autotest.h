#pragma once

// Autotest mode: runs a scenario file of console commands and test directives
// unattended, then writes a JSON result and exits with a meaningful exit code.
//
//   xrEngine.exe -autotest <scenario> [-autotest_out <result.json>] [-autotest_timeout <s>]
//
// The scenario path is relative to the current folder (the game root) or absolute,
// and may not contain spaces. -autotest implies -unattended (see xrDebug.h): fatal errors
// never show a dialog. The window keeps rendering and the game never pauses when it is
// in the background. The format and the result are described in tests/autotest/README.md.
// Ported from the X-Ray 1.0 (Shadow of Chornobyl) engine fork.
//
// Exit codes: 0 passed, 1 failed (or quit early), 2 bad scenario, 3 crashed,
//             4 timed out or hung.

// The game DLL's half of the ui_* steps (xrGame/autotest_ui.cpp): it knows the UI windows
// and the key names. The game DLL registers it with Autotest::set_ui when it loads.
class IAutotestUI
{
public:
	typedef void	(*line_cb)		(void* ctx, LPCSTR line);

	// A visible window by a target path: segments such as a name, option:, item:, st:, class:,
	// text: or #n, separated by '/' (tests/autotest/README.md has the rules).
	// Writes its centre in UI coordinates (1024x768); on failure, why into error.
	virtual bool	find_window		(LPCSTR target, Fvector2& center, LPSTR error, u32 error_size)	= 0;
	virtual void	dump			(line_cb cb, void* ctx)											= 0;	// the visible windows, one line each
	virtual int		key_code		(LPCSTR name)													= 0;	// key (kESCAPE, ESCAPE, mouse1) or action (inventory); -1 unknown
	virtual bool	cursor_visible	()																= 0;
};

namespace Autotest
{
	ENGINE_API	bool	active	();	// -autotest is on the command line

	// After Core._initialize (FS ready): reads the scenario, installs the log observer,
	// the crash callback and the watchdog. False when the scenario cannot be used:
	// the caller then skips the game and returns Finish().
	bool	Begin			();
	void	Start			();		// after the device and CApplication exist: runs the scenario
	void	Stop			();		// after Device.Run returns (shutdown starts)
	void	OnFrameEnd		();		// the frame is complete, debug overlays included (before the present)
	int		Finish			();		// after Core._destroy: writes the final result, returns the exit code

	// The UI cursor: in autotest mode the game uses a virtual cursor that only the ui_*
	// steps move, so the scenario never depends on (or moves) the real mouse.
	ENGINE_API	void	set_ui			(IAutotestUI* ui);
	ENGINE_API	bool	ui_cursor		(Fvector2& pos);		// true in autotest mode: the cursor position in UI coordinates
	ENGINE_API	void	set_ui_cursor	(const Fvector2& pos);	// the game moved the cursor itself
};
