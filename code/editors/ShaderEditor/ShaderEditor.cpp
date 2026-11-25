// Shader Editor entry point.
#include "stdafx.h"
#include "../../engine/xrEngine/xr_input.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* pCmdLine, int nCmdShow)
{
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		Msg("! SDL_Init Error: %s", SDL_GetError());
		return 0;
	}

	// Infernis: no splash screen (IX-Ray's comes from its launcher, xrPlay).
	Debug._initialize();

    const char* FSName = "fs.ltx";
    const char* fsgame_ltx_name = "-fsltx ";
    string_path fsgame = "";

    if (strstr(pCmdLine, fsgame_ltx_name)) {
        int						sz = xr_strlen(fsgame_ltx_name);
        sscanf(strstr(pCmdLine, fsgame_ltx_name) + sz, "%[^ ] ", fsgame);
    }

    Core._initialize("Shader", ELogCallback, true, fsgame[0] ? fsgame : FSName);

	Tools = new CShaderTool();
	STools = (CShaderTool*)Tools;

	UI = new CShaderMain();
	UI->RegisterCommands();

	UIMainForm* MainForm = new UIMainForm();
	::MainForm = MainForm;
	UI->Push(MainForm, false);


	// Infernis: unattended checks. -frames <n> closes the editor after n frames; -shot <file.png|bmp> saves the
	// last of them (a path relative to the working folder).
	int FrameLimit = 0;
	string_path ShotPath = "";
	if (const char* Arg = strstr(pCmdLine, "-frames "))
		FrameLimit = atoi(Arg + 8);
	if (const char* Arg = strstr(pCmdLine, "-shot "))
		sscanf(Arg + 6, "%[^ ]", ShotPath);
	int FrameCount = 0;

	bool NeedExit = false;
	while (!NeedExit)
	{
		SDL_Event Event;
		while (SDL_PollEvent(&Event))
		{
			switch (Event.type)
			{
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
				SDL_WindowID MainWndID = SDL_GetWindowID(g_AppInfo.Window);
				if (Event.window.windowID == MainWndID)
				{
					EPrefs->SaveConfig();
					NeedExit = true;
				}
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED:
			{
				SDL_WindowID MainWndID = SDL_GetWindowID(g_AppInfo.Window);
				if (UI && REDevice && Event.window.windowID == MainWndID)
				{
					UI->Resize(Event.window.data1, Event.window.data2, true);
					EPrefs->SaveConfig();
				}
				break;
			}
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				Device.b_is_Active = true;
				//if (UI) UI->OnAppActivate();

				break;
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				Device.b_is_Active = false;
				//if (UI)UI->OnAppDeactivate();
				break;

			case SDL_EVENT_KEY_DOWN:
				if (UI)UI->KeyDown(Event.key.scancode, UI->GetShiftState());
				break;
			case SDL_EVENT_KEY_UP:
				if (UI)UI->KeyUp(Event.key.scancode, UI->GetShiftState());
				break;

			// Infernis: the mouse reaches the viewport through the engine's DirectInput device (CInput polls it each
			// frame, as in GSC's editors); IX-Ray fed SDL mouse events into its own CInput instead.
			}

			if (!UI->ProcessEvent(&Event))
				break;
		}
		if (FrameLimit > 0 && ShotPath[0] && FrameCount == FrameLimit - 1)
			EDevice->RequestScreenshot(ShotPath);
		MainForm->Frame();
		if (FrameLimit > 0 && ++FrameCount >= FrameLimit)
			NeedExit = true;
	}
	xr_delete(MainForm);
	Core._destroy();
	return 0;
}
