#include "stdafx.h"
#include "../xrRender/RenderDeviceRender.h"

#include "xr_autotest.h"
#include "XR_IOConsole.h"
#include "IGame_Level.h"
#include "x_ray.h"
#include "xr_input.h"

#include <dbghelp.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>

#pragma comment(lib,"psapi.lib")

// Plain std containers and Win32 primitives on purpose: the final result is written
// after Core._destroy, and the crash/hang paths must not depend on engine state.

extern ENGINE_API BOOL g_appLoaded;

namespace
{
	enum EStatus	{ stRunning, stPassed, stFailed, stError, stCrashed, stTimeout };
	enum EPhase		{ phStartup, phScenario, phShutdown, phDone };

	const char*	status_name	(EStatus s)
	{
		switch (s) {
		case stRunning:	return "running";
		case stPassed:	return "passed";
		case stFailed:	return "failed";
		case stError:	return "error";
		case stCrashed:	return "crashed";
		case stTimeout:	return "timeout";
		}
		return "unknown";
	}
	const char*	phase_name	(EPhase p)
	{
		switch (p) {
		case phStartup:		return "startup";
		case phScenario:	return "scenario";
		case phShutdown:	return "shutdown";
		case phDone:		return "done";
		}
		return "unknown";
	}
	int			exit_code_of(EStatus s)
	{
		switch (s) {
		case stPassed:	return 0;
		case stError:	return 2;
		case stCrashed:	return 3;
		case stTimeout:	return 4;
		default:		return 1;	// failed, or still running when the game quit
		}
	}

	double		now_s		()
	{
		static LARGE_INTEGER	freq = {0};
		if (!freq.QuadPart)		QueryPerformanceFrequency(&freq);
		LARGE_INTEGER			t;
		QueryPerformanceCounter	(&t);
		return					double(t.QuadPart)/double(freq.QuadPart);
	}

	std::string	trim		(const std::string& s)
	{
		size_t b = s.find_first_not_of(" \t\r\n");
		if (b==std::string::npos)	return "";
		size_t e = s.find_last_not_of(" \t\r\n");
		return s.substr(b,e-b+1);
	}

	std::string	full_path	(const char* path)
	{
		char	buf[MAX_PATH*2];
		DWORD	n = GetFullPathNameA(path,sizeof(buf),buf,NULL);
		return	(n && n<sizeof(buf)) ? std::string(buf) : std::string(path);
	}

	void		json_str	(std::string& out, const std::string& s)
	{
		out		+= '"';
		for (size_t i=0; i<s.size(); ++i) {
			unsigned char c = (unsigned char)s[i];
			switch (c) {
			case '"':	out += "\\\"";	break;
			case '\\':	out += "\\\\";	break;
			case '\n':	out += "\\n";	break;
			case '\r':	out += "\\r";	break;
			case '\t':	out += "\\t";	break;
			default:
				if (c<0x20)		{ char b[8]; xr_sprintf(b,"\\u%04x",c); out += b; }
				else if (c<0x80)	out += char(c);
				else			{ char b[8]; xr_sprintf(b,"\\u%04x",c); out += b; }	// cp1251 bytes: keep them readable as \u00XX
			}
		}
		out		+= '"';
	}

	// Command-line value after "<key> " (Core.Params is lower case); "" if absent
	std::string	param		(const char* key)
	{
		std::string	k		= std::string(key) + " ";
		const char*	p		= strstr(Core.Params,k.c_str());
		if (!p)				return "";
		p					+= k.size();
		while (*p==' ')		++p;
		std::string	v;
		while (*p && *p!=' ')	v += *p++;
		return v;
	}

	struct SStep
	{
		int			line;
		std::string	text;		// the line as written
		std::string	cmd;		// first word
		std::string	arg;		// the rest
	};

	struct SPerf
	{
		std::string	label;
		u32			frames;
		double		seconds;
		double		avg_ms, p50_ms, p95_ms, p99_ms, max_ms;
	};

	const u32	max_error_samples	= 30;
	const u32	log_tail_size		= 30;
	const double hang_seconds		= 180.0;	// no frame for this long - hung (level loading still counts frames)
	const double shutdown_seconds	= 60.0;		// the shutdown after quit must finish within this

	// UI coordinates (xrGame/ui_defs.h) and the key codes of the mouse buttons (xrEngine/xr_level_controller.h)
	const float	ui_width			= 1024.f;
	const float	ui_height			= 768.f;
	const int	mouse_key_first		= 0xED + 100;
	const int	drag_moves			= 8;		// cursor moves (frames) between the press and the release
	const double press_gap			= 0.3;		// between separate presses: the UI turns two left presses within
												// 250 ms into a double click (CUIWindow::OnMouse), and the menus
												// run at thousands of frames per second

	IAutotestUI*	g_ui			= NULL;		// from the game DLL
	Fvector2		g_cursor		= { ui_width/2, ui_height/2 };

	// One input event of a ui_* step; each takes a frame, a wait takes its time
	struct SInput
	{
		enum EType	{ tMove, tButton, tWheel, tKey, tWait };
		EType		type;
		Fvector2	pos;		// tMove
		int			code;		// tButton: 0..2, tWheel: direction, tKey: key code
		bool		down;		// tButton, tKey
		double		seconds;	// tWait: how long; tButton press: the least time since the previous press
	};

	class CAutotest : public pureFrame, public pureRender
	{
	public:
		CRITICAL_SECTION			cs;			// test state, shared with the watchdog and the crash path
		CRITICAL_SECTION			log_cs;		// the log containers only. Lock order: cs, then the engine's
												// log lock, then log_cs; never log while holding log_cs

		// setup
		std::string					scenario_path;
		std::string					result_path;
		std::string					log_path;
		std::string					screenshot_dir;
		double						timeout_s;
		std::vector<SStep>			steps;

		// progress
		EStatus						status;
		EPhase						phase;
		std::string					reason;
		std::string					dump_path;
		int							step;			// index of the current step
		bool						step_started;	// the current step has done its action and now waits
		double						t_start;
		double						t_step;			// when the current step started
		double						t_shutdown;
		u32							frames;
		volatile u32				last_frame_seen;
		double						last_frame_time;
		bool						registered;
		volatile LONG				finished;		// the watchdog stops; nothing else is written

		// waits
		u32							wait_frames_left;
		bool						screenshot_pending;
		std::string					screenshot_name;

		// perf
		bool						perf_on;
		std::string					perf_label;
		double						perf_t0;
		double						last_frame_t;
		std::vector<float>			perf_ms;
		std::vector<SPerf>			perf;

		// log
		std::vector<std::string>	log_lines;		// since Begin()
		std::vector<std::string>	log_new;		// from the observer, moved to log_lines each frame
		std::vector<std::string>	fail_patterns;
		u32							log_errors;
		std::vector<std::string>	error_samples;
		std::vector<std::string>	screenshots;
		std::string					level_name;

		// ui
		std::vector<SInput>			input;			// the events of the current ui_* step
		size_t						input_pos;
		double						input_t;		// when the current wait started
		double						last_press_t;	// when a mouse button was last pressed
		double						hover_s;		// the pause between moving onto a window and pressing
		std::vector<std::string>	ui_dumps;

		CAutotest() : timeout_s(900), status(stRunning), phase(phStartup), step(0), t_start(now_s()), t_step(0), t_shutdown(0),
			step_started(false), frames(0), last_frame_seen(0), last_frame_time(now_s()), registered(false), finished(0), wait_frames_left(0),
			screenshot_pending(false), perf_on(false), perf_t0(0), last_frame_t(0), log_errors(0), input_pos(0), input_t(0), last_press_t(0), hover_s(0.1)
		{
			InitializeCriticalSection	(&cs);
			InitializeCriticalSection	(&log_cs);
		}

		// ---------------------------------------------------------------------------- result
		void	build_json		(std::string& out)
		{
			PROCESS_MEMORY_COUNTERS	pmc;	ZeroMemory(&pmc,sizeof(pmc));
			GetProcessMemoryInfo	(GetCurrentProcess(),&pmc,sizeof(pmc));

			char	b[512];
			out		= "{\n  \"scenario\": ";		json_str(out,scenario_path);
			out		+= ",\n  \"status\": ";			json_str(out,status_name(status));
			xr_sprintf(b,",\n  \"exit_code\": %d",exit_code_of(status));	out += b;
			out		+= ",\n  \"phase\": ";			json_str(out,phase_name(phase));
			out		+= ",\n  \"reason\": ";			json_str(out,reason);
			xr_sprintf(b,",\n  \"step\": %d,\n  \"steps_total\": %d",step < int(steps.size()) ? step+1 : int(steps.size()),int(steps.size()));	out += b;
			out		+= ",\n  \"step_text\": ";		json_str(out,step < int(steps.size()) ? steps[step].text : std::string(""));
			xr_sprintf(b,",\n  \"step_line\": %d",step < int(steps.size()) ? steps[step].line : 0);	out += b;
			xr_sprintf(b,",\n  \"elapsed_s\": %.2f,\n  \"frames\": %u",now_s()-t_start,frames);	out += b;
			out		+= ",\n  \"level\": ";			json_str(out,level_name);
			out		+= ",\n  \"log\": ";			json_str(out,log_path);
			out		+= ",\n  \"dump\": ";			json_str(out,dump_path);
			xr_sprintf(b,",\n  \"peak_working_set_mb\": %.1f,\n  \"peak_commit_mb\": %.1f",
				double(pmc.PeakWorkingSetSize)/(1024.0*1024.0),double(pmc.PeakPagefileUsage)/(1024.0*1024.0));	out += b;

			out		+= ",\n  \"screenshots\": [";
			for (size_t i=0; i<screenshots.size(); ++i) { out += i ? ", " : ""; json_str(out,screenshots[i]); }
			out		+= "]";

			out		+= ",\n  \"ui_dumps\": [";
			for (size_t i=0; i<ui_dumps.size(); ++i) { out += i ? ", " : ""; json_str(out,ui_dumps[i]); }
			out		+= "]";
			xr_sprintf(b,",\n  \"ui_cursor\": [%.0f, %.0f]",g_cursor.x,g_cursor.y);	out += b;

			out		+= ",\n  \"perf\": [";
			for (size_t i=0; i<perf.size(); ++i) {
				const SPerf& P = perf[i];
				out	+= i ? ",\n    {" : "\n    {";
				out	+= "\"label\": ";	json_str(out,P.label);
				xr_sprintf(b,", \"frames\": %u, \"seconds\": %.2f, \"avg_fps\": %.1f, \"avg_ms\": %.2f, \"p50_ms\": %.2f, \"p95_ms\": %.2f, \"p99_ms\": %.2f, \"max_ms\": %.2f}",
					P.frames,P.seconds,P.seconds>0 ? P.frames/P.seconds : 0.0,P.avg_ms,P.p50_ms,P.p95_ms,P.p99_ms,P.max_ms);
				out	+= b;
			}
			out		+= perf.empty() ? "]" : "\n  ]";

			EnterCriticalSection	(&log_cs);
			xr_sprintf(b,",\n  \"log_errors\": %u",log_errors);	out += b;
			out		+= ",\n  \"log_error_samples\": [";
			for (size_t i=0; i<error_samples.size(); ++i) { out += i ? ",\n    " : "\n    "; json_str(out,error_samples[i]); }
			out		+= error_samples.empty() ? "]" : "\n  ]";

			out		+= ",\n  \"log_tail\": [";
			size_t	from = log_lines.size()>log_tail_size ? log_lines.size()-log_tail_size : 0;
			for (size_t i=from; i<log_lines.size(); ++i) { out += i>from ? ",\n    " : "\n    "; json_str(out,log_lines[i]); }
			out		+= log_lines.size()>from ? "\n  ]" : "]";
			LeaveCriticalSection	(&log_cs);
			out		+= "\n}\n";
		}

		// Write to a temporary file and move it over the result, so readers never see half a file
		void	write_result	()
		{
			if (result_path.empty())	return;
			std::string		json;
			build_json		(json);
			std::string		tmp = result_path + ".tmp";
			HANDLE			h = CreateFileA(tmp.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
			if (h==INVALID_HANDLE_VALUE)	return;
			DWORD			written = 0;
			WriteFile		(h,json.data(),DWORD(json.size()),&written,NULL);
			CloseHandle		(h);
			MoveFileExA		(tmp.c_str(),result_path.c_str(),MOVEFILE_REPLACE_EXISTING);
		}

		// From threads that may race the main thread: wait a little for the lock, then go anyway
		bool	try_lock		(DWORD ms)
		{
			for (DWORD t=0; t<ms; t+=10) {
				if (TryEnterCriticalSection(&cs))	return true;
				Sleep(10);
			}
			return false;
		}

		// ---------------------------------------------------------------------------- scenario
		bool	load_scenario	()
		{
			FILE*	f = NULL;
			if (fopen_s(&f,scenario_path.c_str(),"rt") || !f) {
				reason	= "cannot open the scenario file";
				return	false;
			}
			char	buf[2048];
			int		line = 0;
			while (fgets(buf,sizeof(buf),f)) {
				++line;
				std::string	s = trim(buf);
				if (s.empty() || s[0]=='#' || (s.size()>1 && s[0]=='/' && s[1]=='/'))	continue;
				SStep		S;
				S.line		= line;
				S.text		= s;
				size_t sp	= s.find_first_of(" \t");
				S.cmd		= sp==std::string::npos ? s : s.substr(0,sp);
				S.arg		= sp==std::string::npos ? "" : trim(s.substr(sp));
				std::transform(S.cmd.begin(),S.cmd.end(),S.cmd.begin(),::tolower);
				steps.push_back(S);
			}
			fclose	(f);
			return	true;
		}

		bool	is_directive	(const std::string& c)
		{
			static const char* names[] = { "wait_frames", "wait_seconds", "wait_level", "wait_log", "expect_log", "fail_on_log",
				"screenshot", "perf_begin", "perf_end", "log", "fail", "quit", "debug_crash", "load_save",
				"ui_move", "ui_click", "ui_rclick", "ui_dblclick", "ui_drag", "ui_down", "ui_up", "ui_wheel", "ui_key", "ui_key_down",
				"ui_key_up", "ui_wait", "ui_expect", "ui_expect_no", "ui_dump", "ui_hover", 0 };
			for (int i=0; names[i]; ++i)	if (c==names[i])	return true;
			return false;
		}

		// Directives need valid arguments, everything else must be a known console command
		bool	validate		()
		{
			for (size_t i=0; i<steps.size(); ++i) {
				SStep&	S = steps[i];
				char	err[512] = "";
				if (S.cmd=="wait_frames" && atoi(S.arg.c_str())<=0)			xr_sprintf(err,"wait_frames needs a frame count");
				else if (S.cmd=="wait_seconds" && atof(S.arg.c_str())<=0)	xr_sprintf(err,"wait_seconds needs a time in seconds");
				else if (S.cmd=="wait_log" && (atof(S.arg.c_str())<=0 || S.arg.find(' ')==std::string::npos))
																			xr_sprintf(err,"wait_log needs <seconds> <text>");
				else if ((S.cmd=="expect_log" || S.cmd=="fail_on_log") && S.arg.empty())	xr_sprintf(err,"%s needs a text",S.cmd.c_str());
				else if (S.cmd=="screenshot" && (S.arg.empty() || S.arg.find_first_of(" \\/:*?\"<>|")!=std::string::npos))
																			xr_sprintf(err,"screenshot needs a file name without spaces or path characters");
				else if (S.cmd=="load_save" && (S.arg.empty() || S.arg.find_first_of(" ()/")!=std::string::npos))
																			xr_sprintf(err,"load_save needs a save name (no spaces, parentheses or slashes)");
				else if (S.cmd=="perf_begin" && S.arg.empty())				xr_sprintf(err,"perf_begin needs a label");
				else if (S.cmd.compare(0,3,"ui_")==0 && !validate_ui(S,err,sizeof(err)))	{}
				else if (S.cmd=="debug_crash" && S.arg!="assert" && S.arg!="access_violation")
																			xr_sprintf(err,"debug_crash needs 'assert' or 'access_violation'");
				else if (!is_directive(S.cmd) && Console->Commands.find(S.cmd.c_str())==Console->Commands.end())
																			xr_sprintf(err,"unknown console command '%s'",S.cmd.c_str());
				if (*err) {
					step	= int(i);
					reason	= err;
					return	false;
				}
			}
			return true;
		}

		static bool	mouse_button	(const std::string& a, int& btn)
		{
			if (a.empty() || a=="left")	btn = 0;
			else if (a=="right")		btn = 1;
			else if (a=="middle")		btn = 2;
			else						return false;
			return true;
		}

		// ui_wait <seconds> <target>: the target part
		static std::string	after_number	(const std::string& a)
		{
			size_t	sp	= a.find(' ');
			return	sp==std::string::npos ? std::string("") : trim(a.substr(sp));
		}

		bool	validate_ui		(const SStep& S, char* err, u32 err_size)
		{
			const std::string&	c	= S.cmd;
			const std::string&	a	= S.arg;
			int					btn;
			if (c=="ui_hover") {
				if (a.empty() || atof(a.c_str())<0)	xr_sprintf(err,err_size,"ui_hover needs a time in seconds");
				return	!*err;
			}
			if (!g_ui)	{ xr_sprintf(err,err_size,"%s: the game DLL has no autotest UI",c.c_str()); return false; }
			if (a.size()>256)	xr_sprintf(err,err_size,"%s: the argument is too long",c.c_str());
			else if ((c=="ui_move" || c=="ui_click" || c=="ui_rclick" || c=="ui_dblclick" || c=="ui_expect" || c=="ui_expect_no") && a.empty())
				xr_sprintf(err,err_size,"%s needs a window name, text:<text> or x,y",c.c_str());
			else if (c=="ui_drag" && a.find(" -> ")==std::string::npos)
				xr_sprintf(err,err_size,"ui_drag needs <from> -> <to>");
			else if ((c=="ui_down" || c=="ui_up") && !mouse_button(a,btn))
				xr_sprintf(err,err_size,"%s takes left, right or middle",c.c_str());
			else if (c=="ui_wheel" && atoi(a.c_str())==0)
				xr_sprintf(err,err_size,"ui_wheel needs a non-zero count (up is positive)");
			else if (c=="ui_wait" && (atof(a.c_str())<=0 || after_number(a).empty()))
				xr_sprintf(err,err_size,"ui_wait needs <seconds> <target>");
			else if ((c=="ui_wait" || c=="ui_expect" || c=="ui_expect_no") && is_point(c=="ui_wait" ? after_number(a) : a))
				xr_sprintf(err,err_size,"%s checks a window: a point (x,y) is always there",c.c_str());
			else if (c=="ui_dump" && a.find_first_of(" \\/:*?\"<>|")!=std::string::npos)
				xr_sprintf(err,err_size,"ui_dump takes a label without spaces or path characters");
			else if (c=="ui_key" || c=="ui_key_down" || c=="ui_key_up") {
				if (a.empty() || a.size()>32 || g_ui->key_code(a.c_str())<0)
					xr_sprintf(err,err_size,"%s: unknown or unbound key '%s' (a key like kESCAPE, ESCAPE, mouse1, or a bound action like inventory)",c.c_str(),a.c_str());
			}
			return	!*err;
		}

		static bool	is_point	(const std::string& target, Fvector2* pos=NULL)
		{
			float	x, y;
			char	rest;
			if (sscanf_s(target.c_str(),"%f,%f%c",&x,&y,&rest,1)!=2)	return false;
			if (pos)	pos->set(x,y);
			return	true;
		}

		// "x,y" in UI coordinates (1024x768), or a window for the game DLL to find
		bool	resolve			(const std::string& target, Fvector2& pos, std::string& err)
		{
			if (is_point(target,&pos))	return true;
			char	e[512]	= "";
			if (g_ui->find_window(target.c_str(),pos,e,sizeof(e)))	return true;
			err		= e;
			return	false;
		}

		void	add_input		(SInput::EType type, const Fvector2& p, int code, bool down, double s)
		{
			SInput	I;
			I.type		= type;
			I.pos		= p;
			I.code		= code;
			I.down		= down;
			I.seconds	= s;
			input.push_back	(I);
		}
		void	add_move		(const Fvector2& p)		{ add_input(SInput::tMove,p,0,false,0);			}
		void	add_button		(int btn, bool down, double gap=press_gap)	{ add_input(SInput::tButton,g_cursor,btn,down,down ? gap : 0);	}
		void	add_key			(int code, bool down)	{ add_input(SInput::tKey,g_cursor,code,down,0);	}
		void	add_wait		(double s)				{ add_input(SInput::tWait,g_cursor,0,false,s);		}
		void	add_click		(const Fvector2& p, int btn, int count)
		{
			add_move	(p);
			if (hover_s>0)	add_wait(hover_s);
			for (int i=0; i<count; ++i) { add_button(btn,true,i ? 0 : press_gap); add_button(btn,false); }	// a double click: no gap
		}

		void	send			(const SInput& I)
		{
			if (!pInput)	return;
			switch (I.type) {
			case SInput::tMove: {
				Fvector2	old	= g_cursor;
				g_cursor		= I.pos;
				clamp			(g_cursor.x,0.f,ui_width);
				clamp			(g_cursor.y,0.f,ui_height);
				pInput->inject_mouse_move	(iFloor((g_cursor.x-old.x)*Device.dwWidth/ui_width),iFloor((g_cursor.y-old.y)*Device.dwHeight/ui_height));
				} break;
			case SInput::tButton:	pInput->inject_mouse_button	(I.code,I.down);	break;
			case SInput::tWheel:	pInput->inject_mouse_wheel	(I.code);			break;
			case SInput::tKey:
				if (I.code>=mouse_key_first && I.code<mouse_key_first+3)	pInput->inject_mouse_button(I.code-mouse_key_first,I.down);
				else														pInput->inject_key(I.code,I.down);
				break;
			default:	break;
			}
		}

		// The events of the current ui_* step, one per frame. True when all are sent.
		bool	run_input		()
		{
			double	t	= now_s();
			while (input_pos<input.size()) {
				const SInput&	I	= input[input_pos];
				if (I.type==SInput::tWait) {
					if (input_t==0)	{ input_t = t; return false; }	// at least a frame
					if (t-input_t < I.seconds)	return false;
					input_t		= 0;
					++input_pos;
					continue;
				}
				if (I.type==SInput::tButton && I.down) {
					if (t-last_press_t < I.seconds)	return false;
					last_press_t	= t;
				}
				send		(I);
				++input_pos;
				return		false;	// the next event next frame, so the UI sees each one
			}
			return true;
		}

		static void	dump_line	(void* ctx, LPCSTR line)
		{
			std::string&	out	= *(std::string*)ctx;
			out		+= line;
			out		+= "\r\n";
		}

		void	ui_dump			(const std::string& label)
		{
			std::string		text;
			g_ui->dump		(dump_line,&text);
			std::string		dir		= result_path;
			size_t			slash	= dir.find_last_of("\\/");
			dir				= slash==std::string::npos ? std::string(".") : dir.substr(0,slash);
			std::string		path	= dir + "\\autotest_ui_" + (label.empty() ? std::string("dump") : label) + ".txt";
			char			head[160];
			xr_sprintf		(head,"# visible UI windows, top-level dialogs first; [x,y wxh] in UI coordinates (%.0fx%.0f); cursor at %.0f,%.0f%s\r\n",
							ui_width,ui_height,g_cursor.x,g_cursor.y,g_ui->cursor_visible() ? "" : " (hidden)");
			text.insert		(0,head);
			FILE*			f		= NULL;
			if (fopen_s(&f,path.c_str(),"wb") || !f) {
				finish_scenario	(stFailed,"ui_dump: cannot write " + path);
				return;
			}
			fwrite			(text.data(),1,text.size(),f);
			fclose			(f);
			ui_dumps.push_back	(path);
			Msg				("* [autotest] ui_dump: %s",path.c_str());
		}

		// A ui_* step. Same contract as run_step.
		bool	run_ui_step		(const SStep& S, double waited)
		{
			const std::string&	c	= S.cmd;
			const std::string&	a	= S.arg;
			if (c=="ui_hover")	{ hover_s = atof(a.c_str()); return true; }
			if (c=="ui_dump")	{ ui_dump(a); return phase==phScenario; }
			if (c=="ui_wait" || c=="ui_expect" || c=="ui_expect_no") {
				std::string	target	= c=="ui_wait" ? after_number(a) : a;
				Fvector2	p;
				std::string	err;
				bool		found	= resolve(target,p,err);
				if (c=="ui_expect_no") {
					if (!found)		return true;
					finish_scenario	(stFailed,"ui_expect_no: '" + target + "' is visible");
					return			false;
				}
				if (found)			return true;
				if (c=="ui_expect" || waited>=atof(a.c_str()))
					finish_scenario	(stFailed,c + ": " + err);
				return				false;
			}

			if (!step_started) {	// build the events
				step_started	= true;
				// Without the UI cursor, mouse events go to the actor: a move turns the camera,
				// a press fires. That is never what a UI step means.
				if ((c=="ui_move" || c=="ui_click" || c=="ui_rclick" || c=="ui_dblclick" || c=="ui_drag") && !g_ui->cursor_visible()) {
					finish_scenario	(stFailed,c + ": the UI cursor is hidden (no menu or dialog takes the mouse)");
					return			false;
				}
				input.clear		();
				input_pos		= 0;
				input_t			= 0;
				Fvector2		p, q;
				std::string		err;
				int				btn		= 0;
				if (c=="ui_move" || c=="ui_click" || c=="ui_rclick" || c=="ui_dblclick") {
					if (!resolve(a,p,err))	{ finish_scenario(stFailed,c + ": " + err); return false; }
					if (c=="ui_move")			add_move	(p);
					else if (c=="ui_click")		add_click	(p,0,1);
					else if (c=="ui_rclick")	add_click	(p,1,1);
					else						add_click	(p,0,2);
				} else if (c=="ui_drag") {
					size_t		arrow	= a.find(" -> ");
					std::string	from	= trim(a.substr(0,arrow));
					std::string	to		= trim(a.substr(arrow+4));
					if (!resolve(from,p,err) || !resolve(to,q,err))	{ finish_scenario(stFailed,"ui_drag: " + err); return false; }
					add_move	(p);
					if (hover_s>0)	add_wait(hover_s);
					add_button	(0,true);
					for (int i=1; i<=drag_moves; ++i) {
						Fvector2	m;
						m.set		(p.x+(q.x-p.x)*i/drag_moves,p.y+(q.y-p.y)*i/drag_moves);
						add_move	(m);
					}
					if (hover_s>0)	add_wait(hover_s);
					add_button	(0,false);
				} else if (c=="ui_down" || c=="ui_up") {
					mouse_button	(a,btn);
					add_button		(btn,c=="ui_down");
				} else if (c=="ui_wheel") {
					int		n	= atoi(a.c_str());
					for (int i=0; i<abs(n); ++i)	add_input(SInput::tWheel,g_cursor,n>0 ? 1 : -1,false,0);
				} else {	// ui_key, ui_key_down, ui_key_up
					int		code	= g_ui->key_code(a.c_str());
					if (c!="ui_key_up")		add_key(code,true);
					if (c!="ui_key_down")	add_key(code,false);
				}
			}
			return run_input();
		}

		// Loaded, the actor exists, and the load screen is gone. After loading, the load
		// screen stays until a key is pressed unless keypress_on_start is 0.
		bool	level_ready		()
		{
			return g_pGameLevel && g_appLoaded && g_pGameLevel->bReady && g_pGameLevel->CurrentEntity()
				&& Device.dwPrecacheFrame==0 && !load_screen_renderer.b_registered;
		}

		bool	log_contains	(const std::string& text)
		{
			bool	found	= false;
			EnterCriticalSection	(&log_cs);
			for (size_t i=0; i<log_lines.size() && !found; ++i)
				found		= log_lines[i].find(text)!=std::string::npos;
			LeaveCriticalSection	(&log_cs);
			return	found;
		}

		void	finish_scenario	(EStatus s, const std::string& why)
		{
			status		= s;
			reason		= why;
			if (perf_on)	perf_end();
			Msg			("%s [autotest] %s%s%s",s==stPassed ? "*" : "!",status_name(s),why.empty() ? "" : ": ",why.c_str());
			phase		= phShutdown;
			t_shutdown	= now_s();
			write_result();
			FlushLog	();
			Console->Execute	("quit");
		}

		void	perf_end		()
		{
			SPerf	P;
			P.label		= perf_label;
			P.frames	= u32(perf_ms.size());
			P.seconds	= now_s()-perf_t0;
			P.avg_ms	= P.p50_ms = P.p95_ms = P.p99_ms = P.max_ms = 0;
			if (!perf_ms.empty()) {
				std::vector<float>	v = perf_ms;
				std::sort	(v.begin(),v.end());
				double		sum = 0;
				for (size_t i=0; i<v.size(); ++i)	sum += v[i];
				P.avg_ms	= sum/v.size();
				P.p50_ms	= v[size_t(0.50*(v.size()-1))];
				P.p95_ms	= v[size_t(0.95*(v.size()-1))];
				P.p99_ms	= v[size_t(0.99*(v.size()-1))];
				P.max_ms	= v.back();
			}
			perf.push_back	(P);
			perf_on			= false;
			perf_ms.clear	();
			Msg				("* [autotest] perf '%s': %u frames, avg %.2f ms, p95 %.2f ms, p99 %.2f ms, max %.2f ms",
				P.label.c_str(),P.frames,P.avg_ms,P.p95_ms,P.p99_ms,P.max_ms);
		}

		void	next_step		()
		{
			++step;
			t_step			= now_s();
			step_started	= false;
			wait_frames_left= 0;
			if (step<int(steps.size())) {
				Msg			("* [autotest] step %d/%d (line %d): %s",step+1,int(steps.size()),steps[step].line,steps[step].text.c_str());
				write_result();
				FlushLog	();
			}
		}

		// Runs the current step. True when it is done (go on with the next one this frame),
		// false when it waits or has handed work to the engine (go on next frame).
		bool	run_step		()
		{
			SStep&	S		= steps[step];
			double	waited	= now_s()-t_step;

			if (S.cmd=="wait_frames") {
				if (!step_started) {
					step_started		= true;
					wait_frames_left	= u32(atoi(S.arg.c_str()));
					return				false;
				}
				return (--wait_frames_left)==0;
			}
			if (S.cmd=="wait_seconds")
				return waited >= atof(S.arg.c_str());
			if (S.cmd=="wait_level") {
				double	limit	= S.arg.empty() ? 300.0 : atof(S.arg.c_str());
				if (level_ready()) {
					level_name	= g_pGameLevel->name().c_str();
					Msg			("* [autotest] level '%s' is ready (%.1f s)",level_name.c_str(),waited);
					return		true;
				}
				if (waited > limit) {
					char	b[192];
					if (g_pGameLevel && g_pGameLevel->bReady && load_screen_renderer.b_registered)
						xr_sprintf(b,"wait_level: the load screen is still up after %.0f s (it waits for a key unless keypress_on_start is 0)",limit);
					else
						xr_sprintf(b,"wait_level: no level after %.0f s",limit);
					finish_scenario(stFailed,b);
				}
				return false;
			}
			if (S.cmd=="wait_log" || S.cmd=="expect_log") {
				std::string	text	= S.arg;
				double		limit	= 0;
				if (S.cmd=="wait_log") {
					size_t	sp	= S.arg.find(' ');
					limit		= atof(S.arg.substr(0,sp).c_str());
					text		= trim(S.arg.substr(sp));
				}
				if (log_contains(text))		return true;
				if (waited >= limit) {
					finish_scenario	(stFailed,std::string(S.cmd) + ": the log has no line with '" + text + "'");
				}
				return false;
			}
			if (S.cmd=="fail_on_log") {
				fail_patterns.push_back	(S.arg);
				return true;
			}
			if (S.cmd=="screenshot") {
				if (!step_started) {		// request; OnRender of this frame takes it
					step_started		= true;
					screenshot_pending	= true;
					screenshot_name		= S.arg;
					return false;
				}
				if (!screenshot_pending)	return true;	// taken (on failure the scenario has already finished)
				if (waited > 10.0)	finish_scenario(stFailed,"screenshot: no frame was rendered within 10 s");
				return false;
			}
			if (S.cmd=="perf_begin") {
				if (perf_on)	perf_end();
				perf_on		= true;
				perf_label	= S.arg;
				perf_t0		= now_s();
				perf_ms.clear();
				return true;
			}
			if (S.cmd=="perf_end") {
				if (perf_on)	perf_end();
				return true;
			}
			if (S.cmd=="log") {
				Msg		("* [autotest] %s",S.arg.c_str());
				return	true;
			}
			if (S.cmd=="fail") {
				finish_scenario	(stFailed,S.arg.empty() ? "fail" : S.arg);
				return false;
			}
			if (S.cmd=="load_save") {
				// 'load' needs a running game; from the main menu a save is loaded by
				// starting a server on it, as the menu does
				if (!step_started) {
					step_started	= true;
					std::string	c	= g_pGameLevel ? "load " + S.arg
									: "start server(" + S.arg + "/single/alife/load) client(localhost)";
					Msg				("* [autotest] %s",c.c_str());
					Console->Execute(c.c_str());
					return			false;
				}
				return true;
			}
			if (S.cmd.compare(0,3,"ui_")==0)
				return run_ui_step(S,waited);
			if (S.cmd=="debug_crash") {	// tests the crash path itself
				Msg		("* [autotest] debug_crash %s",S.arg.c_str());
				FlushLog();
				if (S.arg=="assert")	R_ASSERT2(0,"autotest debug_crash");
				else					*(volatile int*)0 = 0;
				return	true;
			}
			if (S.cmd=="quit") {
				finish_scenario	(stPassed,"");
				return false;
			}

			// A console command; give the engine a frame to process what it started
			if (!step_started) {
				step_started		= true;
				std::string	c		= S.arg.empty() ? S.cmd : S.cmd + " " + S.arg;
				Console->Execute	(c.c_str());
				return				false;
			}
			return true;
		}

		void	drain_log		()
		{
			std::vector<std::string>	lines;
			EnterCriticalSection	(&log_cs);
			lines.swap				(log_new);
			LeaveCriticalSection	(&log_cs);

			for (size_t i=0; i<lines.size(); ++i) {
				const std::string&	L = lines[i];
				if (phase==phScenario)
					for (size_t p=0; p<fail_patterns.size(); ++p)
						if (L.find(fail_patterns[p])!=std::string::npos && L.find("[autotest]")==std::string::npos) {
							finish_scenario	(stFailed,"fail_on_log: " + L);
							break;
						}
			}
		}

		// ---------------------------------------------------------------------------- pureFrame / pureRender
		virtual void	OnFrame		()
		{
			double	t	= now_s();
			if (perf_on && last_frame_t>0)	perf_ms.push_back(float((t-last_frame_t)*1000.0));
			last_frame_t	= t;
			++frames;

			EnterCriticalSection	(&cs);
			drain_log	();
			for (int guard=0; guard<64 && phase==phScenario; ++guard) {
				if (step>=int(steps.size())) {
					finish_scenario	(stPassed,"");	// the scenario ended without quit
					break;
				}
				if (!run_step())	break;
				if (phase==phScenario)	next_step();
			}
			LeaveCriticalSection	(&cs);
		}

		virtual void	OnRender	()
		{
			if (!screenshot_pending)	return;
			EnterCriticalSection	(&cs);
			std::string		path	= screenshot_dir + "\\" + screenshot_name + ".png";
			const char*		err		= capture(path.c_str());
			screenshot_pending		= false;
			if (err)	finish_scenario(stFailed,std::string("screenshot: ") + err);
			else {
				screenshots.push_back	(path);
				Msg						("* [autotest] screenshot: %s",path.c_str());
			}
			LeaveCriticalSection	(&cs);
		}

		// The finished frame (HUD, UI and console included; not the UI cursor and the stats,
		// which are drawn later) from the back buffer, as PNG.
		// Works in windowed mode, unlike the 'screenshot' console command.
		const char*		capture		(const char* path)
		{
#ifdef DEDICATED_SERVER
			return			"the dedicated server does not render";
#endif
			return			Device.m_pRender->SaveBackBufferPNG(path);
		}
	};

	CAutotest*	g_test	= NULL;		// deliberately never deleted: the crash path may run at any time

	// ---------------------------------------------------------------------------- callbacks
	void	log_observer	(LPCSTR line)
	{
		CAutotest*	T	= g_test;
		if (!T || !line)	return;
		EnterCriticalSection	(&T->log_cs);
		T->log_lines.push_back	(line);
		T->log_new.push_back	(line);
		if (line[0]=='!' && !strstr(line,"[autotest]") && !strstr(line,"[unattended]")) {
			++T->log_errors;
			if (T->error_samples.size()<max_error_samples)	T->error_samples.push_back(line);
		}
		LeaveCriticalSection	(&T->log_cs);
	}

	void	on_crash		(const char* reason, const char* dump)
	{
		CAutotest*	T	= g_test;
		if (!T)			return;
		bool	locked	= T->try_lock(2000);
		T->status		= stCrashed;
		T->reason		= reason;
		T->dump_path	= dump;
		T->write_result	();
		InterlockedExchange	(&T->finished,1);
		if (locked)		LeaveCriticalSection(&T->cs);
	}

	void	write_hang_dump	(CAutotest* T)
	{
		HMODULE	dll		= LoadLibraryA("dbghelp.dll");
		if (!dll)		return;
		typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE,DWORD,HANDLE,MINIDUMP_TYPE,PMINIDUMP_EXCEPTION_INFORMATION,PMINIDUMP_USER_STREAM_INFORMATION,PMINIDUMP_CALLBACK_INFORMATION);
		MINIDUMPWRITEDUMP	dump = (MINIDUMPWRITEDUMP)GetProcAddress(dll,"MiniDumpWriteDump");
		if (!dump)		return;
		std::string		path	= T->result_path;
		size_t			dot		= path.rfind('.');
		path			= (dot==std::string::npos ? path : path.substr(0,dot)) + "_hang.mdmp";
		HANDLE			h		= CreateFileA(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if (h==INVALID_HANDLE_VALUE)	return;
		if (dump(GetCurrentProcess(),GetCurrentProcessId(),h,MINIDUMP_TYPE(MiniDumpNormal|MiniDumpWithThreadInfo|MiniDumpScanMemory),NULL,NULL,NULL))
			T->dump_path	= path;
		CloseHandle		(h);
	}

	void	watchdog		()
	{
		CAutotest*	T	= g_test;
		while (!T->finished) {
			Sleep		(500);
			if (T->finished)	break;

			double	t	= now_s();
			if (Device.dwFrame!=T->last_frame_seen) {
				T->last_frame_seen	= Device.dwFrame;
				T->last_frame_time	= t;
			}

			const char*	why	= NULL;
			char		b[256];
			if (t-T->t_start > T->timeout_s) {
				xr_sprintf(b,"timeout: the test did not finish in %.0f s (-autotest_timeout)",T->timeout_s);	why = b;
			} else if (T->phase==phShutdown && t-T->t_shutdown > shutdown_seconds) {
				xr_sprintf(b,"hang: the shutdown did not finish in %.0f s",shutdown_seconds);	why = b;
			} else if (T->phase!=phShutdown && T->phase!=phDone && t-T->last_frame_time > hang_seconds) {
				xr_sprintf(b,"hang: no frame for %.0f s",hang_seconds);	why = b;
			}
			if (!why)	continue;

			bool	locked	= T->try_lock(2000);
			if (T->finished) { if (locked) LeaveCriticalSection(&T->cs); break; }
			InterlockedExchange	(&T->finished,1);
			Msg				("! [autotest] %s, terminating (exit code 4)",why);
			T->status		= stTimeout;
			T->reason		= why;
			write_hang_dump	(T);
			T->write_result	();
			if (locked)		LeaveCriticalSection(&T->cs);
			FlushLog		();
			TerminateProcess(GetCurrentProcess(),4);
		}
	}
}

bool Autotest::active()
{
	static int	result	= -1;
	if (result<0)	result = strstr(Core.Params,"-autotest ") ? 1 : 0;
	return		(result==1);
}

void Autotest::set_ui(IAutotestUI* ui)
{
	g_ui		= ui;
}

bool Autotest::ui_cursor(Fvector2& pos)
{
	if (!active())	return false;
	pos			= g_cursor;
	return		true;
}

void Autotest::set_ui_cursor(const Fvector2& pos)
{
	g_cursor	= pos;
}

bool Autotest::Begin()
{
	if (!active())	return true;

	CAutotest*	T	= xr_new<CAutotest>();
	g_test			= T;

	T->scenario_path	= full_path(param("-autotest").c_str());

	string_path		p;
	std::string		out	= param("-autotest_out");
	if (out.empty()) {
		FS.update_path	(p,"$logs$","autotest_result.json");
		out			= p;
	}
	T->result_path	= full_path(out.c_str());
	T->log_path		= full_path(::log_path());

	FS.update_path	(p,"$screenshots$","");
	CreateDirectoryA(p,NULL);
	T->screenshot_dir	= full_path(p);
	if (!T->screenshot_dir.empty() && T->screenshot_dir[T->screenshot_dir.size()-1]=='\\')
		T->screenshot_dir.erase(T->screenshot_dir.size()-1);
	T->screenshot_dir	+= "\\autotest";
	CreateDirectoryA(T->screenshot_dir.c_str(),NULL);

	std::string		timeout	= param("-autotest_timeout");
	if (!timeout.empty() && atof(timeout.c_str())>0)	T->timeout_s = atof(timeout.c_str());

	SetLogObserver				(log_observer);
	Debug.set_on_unattended_crash	(on_crash);

	Msg				("* [autotest] scenario: %s",T->scenario_path.c_str());
	Msg				("* [autotest] result:   %s",T->result_path.c_str());

	if (!T->load_scenario()) {
		T->status	= stError;
		Msg			("! [autotest] error: %s",T->reason.c_str());
		T->write_result	();
		return		false;
	}
	T->write_result	();
	std::thread		(watchdog).detach();
	return			true;
}

void Autotest::Start()
{
	CAutotest*	T	= g_test;
	if (!T)			return;

	// The console commands exist now (xrGame registers its own in CGamePersistent)
	EnterCriticalSection	(&T->cs);
	if (!T->validate()) {
		T->status	= stError;
		Msg			("! [autotest] error at line %d: %s",T->steps[T->step].line,T->reason.c_str());
		T->phase	= phShutdown;
		T->t_shutdown	= now_s();
		T->write_result	();
		Console->Execute("quit");
		LeaveCriticalSection	(&T->cs);
		return;
	}

	Device.b_is_Active	= TRUE;		// render and run even if the window never gets the focus
	Device.seqFrame.Add	(T,REG_PRIORITY_LOW);
	Device.seqRender.Add(T,0);		// after everything else, the console (1) included
	T->registered		= true;

	T->phase		= phScenario;
	T->step			= -1;
	T->next_step	();
	LeaveCriticalSection	(&T->cs);
}

void Autotest::Stop()
{
	CAutotest*	T	= g_test;
	if (!T)			return;
	EnterCriticalSection	(&T->cs);
	if (T->registered) {
		Device.seqFrame.Remove	(T);
		Device.seqRender.Remove	(T);
		T->registered			= false;
	}
	if (T->phase!=phShutdown) {	// the game quit before the scenario did
		T->phase		= phShutdown;
		T->t_shutdown	= now_s();
		if (T->status==stRunning) {
			T->status	= stFailed;
			T->reason	= "the game quit before the scenario finished";
			Msg			("! [autotest] %s",T->reason.c_str());
		}
	}
	T->write_result	();
	LeaveCriticalSection	(&T->cs);
}

int Autotest::Finish()
{
	CAutotest*	T	= g_test;
	if (!T)			return 0;
	EnterCriticalSection	(&T->cs);
	T->phase		= phDone;
	InterlockedExchange	(&T->finished,1);
	SetLogObserver	(NULL);
	T->write_result	();
	int		code	= exit_code_of(T->status);
	Msg				("* [autotest] done: %s, exit code %d",status_name(T->status),code);
	LeaveCriticalSection	(&T->cs);
	return			code;
}
