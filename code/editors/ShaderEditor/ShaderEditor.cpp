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
    string_path fsgame = "";
    ReadCommandLineValue(pCmdLine, "-fsltx ", fsgame); // Infernis: bounded, quotes allowed (IX-Ray: sscanf)

    Core._initialize("Shader", ELogCallback, true, fsgame[0] ? fsgame : FSName);

	// Infernis: unattended checks (-frames, -shot, -save; see EditorCommandLine.h), before the tools and the main
	// form are created, so that dialogs during start-up are covered.
	CEditorCheckRun CheckRun;
	CheckRun.Parse(pCmdLine);

	Tools = new CShaderTool();
	STools = (CShaderTool*)Tools;

	UI = new CShaderMain();
	UI->RegisterCommands();

	UIMainForm* MainForm = new UIMainForm();
	::MainForm = MainForm;
	UI->Push(MainForm, false);


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
					// Infernis: the device takes the window's size in pixels (the event's size is in points)
					int PixelsW = 0, PixelsH = 0;
					SDL_GetWindowSizeInPixels(g_AppInfo.Window, &PixelsW, &PixelsH);
					UI->Resize(PixelsW, PixelsH, true);
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
		CheckRun.BeforeFrame();
		MainForm->Frame();
		if (CheckRun.AfterFrame())
			NeedExit = true;
	}
	xr_delete(MainForm);
	Core._destroy();
	return 0;
}
