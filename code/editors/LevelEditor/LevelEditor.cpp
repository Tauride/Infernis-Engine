// Level Editor entry point.
#include "stdafx.h"

#include "Editor/Utils/ContentView.h"
#include "Editor/Scene/LEPhysics.h"

#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/XR_IOConsole.h"
#include "xrEngine/xr_input.h"

ECORE_API extern bool bIsLevelEditor;
void DragDrop(const xr_string&, int);

// Infernis: written like the other ported editors' entry points. IX-Ray's also shows its launcher's splash screen,
// loads the game DLL (xrGame) for the game persistent object and play in editor (EDITOR_PLAY_IN_EDITOR), and counts
// FPS with its own counter; the editor core (xrECore) gives the editors their game persistent object here.
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* pCmdLine, int nCmdShow)
{
	bIsLevelEditor = true;

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		Msg("! SDL_Init Error: %s", SDL_GetError());
		return 0;
	}

	Debug._initialize();

	const char* FSName = "fs.ltx";
	string_path fsgame = "";
	ReadCommandLineValue(pCmdLine, "-fsltx ", fsgame); // Infernis: bounded, quotes allowed (IX-Ray: sscanf)

	CoInitialize(nullptr);

	Core._initialize("Level", ELogCallback, true, fsgame[0] ? fsgame : FSName);

	// Infernis: unattended checks (-frames, -shot, -save; see EditorCommandLine.h), before the tools and the main
	// form are created, so that dialogs during start-up are covered.
	CEditorCheckRun CheckRun;
	CheckRun.Parse(pCmdLine);

	psDeviceFlags.set(rsFullscreen, false);

	Tools = new CLevelTool();
	LTools = static_cast<CLevelTool*>(Tools);

	UI = new CLevelMain();
	UI->RegisterCommands();
	UI->GeneralTabs.push_back({ "Scene View", nullptr });

	LUI = static_cast<CLevelMain*>(UI);

	Scene = new EScene();

	GContentView = new CContentView;

	UIMainForm* MainForm = new UIMainForm();

	// The server entity factory, Infernis's built for the editors (xrSE_Factory_Editor)
	g_SEFactoryManager = new XrSEFactoryManager();

	PGMLib->Load();

	::MainForm = MainForm;
	UI->Push(MainForm, false);

	MainForm->GetRenderForm()->DragFunctor = DragDrop;

	GContentView->Init();
	UI->PushBegin(GContentView);

	// A level named on the command line (-level <name>, as in the level list, e.g. "jupiter").
	string_path LevelName = "";
	if (ReadCommandLineValue(pCmdLine, "-level ", LevelName))
		ExecCommand(COMMAND_LOAD, xr_string(LevelName));

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
					GContentView->Destroy();
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
				break;
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				Device.b_is_Active = false;
				break;

			case SDL_EVENT_KEY_DOWN:
				if (UI)
				{
					UI->KeyDown(Event.key.scancode, UI->GetShiftState());
					UI->ApplyShortCutInput(Event.key.scancode);
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (UI)
					UI->KeyUp(Event.key.scancode, UI->GetShiftState());
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
		{
			GContentView->Destroy();
			NeedExit = true;
		}
	}

	g_scene_physics.DestroyAll();
	g_scene_physics.DestroyObjectSpace();

	xr_delete(MainForm);
	xr_delete(g_SEFactoryManager);
	Core._destroy();
	return 0;
}
