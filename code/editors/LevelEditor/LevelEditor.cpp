// Level Editor entry point.
#include "stdafx.h"

#include "Editor/Utils/ContentView.h"
#include "Editor/Scene/LEPhysics.h"
#include "Editor/Builder/Builder.h"
#include "UI/UIEditLibrary.h"

#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/XR_IOConsole.h"
#include "xrEngine/xr_input.h"

ECORE_API extern bool bIsLevelEditor;
void DragDrop(const xr_string&, int);

// -make_lods (with -level): the LOD textures (textures\lod\lod_*) of the scene's multiple usage objects, made by
// the object library's Make LOD (high quality); the build's validation needs them.
static void MakeSceneLODs(bool Async)
{
	xr_set<shared_str> Names;
	for (CCustomObject* Custom : Scene->ListObj(OBJCLASS_SCENEOBJECT))
	{
		CSceneObject* SceneObject = static_cast<CSceneObject*>(Custom);
		CEditableObject* Object = SceneObject->GetReference();
		if (Object && Object->IsMUStatic())
			Names.insert(SceneObject->RefName());
	}
	if (Async) // -make_lods_async: as the library's Make LOD button, in steps across frames with the progress window
	{
		UIEditLibrary::StartLODs(SharedStrVec(Names.begin(), Names.end()));
		Msg("* -make_lods_async: %u objects", (u32)Names.size());
		return;
	}
	UIEditLibrary::MakeLODs(SharedStrVec(Names.begin(), Names.end()));
	Msg("* -make_lods: %u objects", (u32)Names.size());
}

// -build (with -level): Compile\Build without its confirmation, on this thread: the static geometry for xrLC, the
// AI map, the game and spawn data, HOM, SOM and each tool's export, into $game_levels$\<level>.
static void BuildScene()
{
	Msg("* -build: %s", Builder.Compile(false, false) ? "done" : "FAILED");
}

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

	// A level named on the command line: -level <name>, a scene under $maps$ without the extension (e.g.
	// "jupiter" or "!FinalSP\jupiter"), or a full path to a .level file. The load command takes a full path.
	string_path LevelName = "";
	bool PendingMakeLods = false, PendingBuild = false, AsyncMakeLods = false;
	if (ReadCommandLineValue(pCmdLine, "-level ", LevelName))
	{
		string_path LevelPath;
		if (FS.exist(LevelName))
			xr_strcpy(LevelPath, LevelName);
		else
			FS.update_path(LevelPath, _maps_, EFS.ChangeFileExt(LevelName, ".level").c_str());
		if (!FS.exist(LevelPath))
			Msg("! -level: no scene '%s' (%s)", LevelName, LevelPath);
		else if (ExecCommand(COMMAND_LOAD, xr_string(LevelPath)))
		{
			PendingMakeLods = HasCommandLineOption(pCmdLine, "-make_lods");
			AsyncMakeLods = HasCommandLineOption(pCmdLine, "-make_lods_async");
			PendingMakeLods |= AsyncMakeLods;
			PendingBuild = HasCommandLineOption(pCmdLine, "-build");
		}
	}
	// -make_lods and -build run after a few frames, as for someone using the editor: objects update their
	// transforms (and other state) on frames, not when they are loaded
	u32 FramesBeforeActions = 5;

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
					UIEditLibrary::LODJobStop();
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
		UIEditLibrary::LODJobUpdate(); // (between frames: it renders the objects' views)

		// (on the check run's last frame at the latest: a -frames limit below 5 must not skip them)
		const bool LastCheckFrame = CheckRun.FrameLimit > 0 && CheckRun.FrameCount + 1 >= CheckRun.FrameLimit;
		if ((PendingMakeLods || PendingBuild) && (--FramesBeforeActions == 0 || LastCheckFrame))
		{
			if (PendingMakeLods)
				MakeSceneLODs(AsyncMakeLods);
			if (PendingBuild)
				BuildScene();
			PendingMakeLods = PendingBuild = false;
		}

		if (CheckRun.AfterFrame())
		{
			UIEditLibrary::LODJobStop();
			GContentView->Destroy();
			NeedExit = true;
		}
	}

	UIEditLibrary::LODJobStop(); // (its worker traces library objects that the shutdown frees)
	g_scene_physics.DestroyAll();
	g_scene_physics.DestroyObjectSpace();

	xr_delete(MainForm);
	xr_delete(g_SEFactoryManager);
	Core._destroy();
	return 0;
}
