// Actor Editor entry point.
#include "stdafx.h"
#include "../../engine/xrEngine/xr_input.h"
#include <shellapi.h> // CommandLineToArgvW

#include "../xrEProps/UIBoneView.h"

namespace
{
void DragFile(xr_string File)
{
	File = Platform::UTF8_to_CP1251(File);

	CActorTools* pTools = (CActorTools*)Tools;
	auto pObject = pTools->CurrentObject();

	auto ErrorMsg = []()
	{
		ELog.DlgMsg(mtError, "Load an object first.");
	};

	if (File.ends_with(".object"))
	{
		ExecCommand(COMMAND_LOAD, File);
	}
	else if (xr_string_contains(File, ".skl"))
	{
		if (pObject != nullptr)
		{
			FS.TryLoad(File.c_str());
			pObject->AppendSMotion(File.c_str());
		}
		else
		{
			ErrorMsg();
		}
		ExecCommand(COMMAND_UPDATE_PROPERTIES);
	}
	else if (File.ends_with(".omf"))
	{
		if (pObject != nullptr)
		{
			string_path CurrentWD = {};
			string_path CurrentGM = {};
			FS.update_path(CurrentGM, "$game_meshes$", "");
			FS.update_path(CurrentWD, "$fs_root$", CurrentGM);

			if (xr_string_contains(File, CurrentWD))
			{
				pObject->m_SMotionRefs.emplace_back(File.substr(strlen(CurrentWD)).c_str());
			}
			else
			{
				xr_path FilePath = File;
				xr_string FileName = FilePath.xfilename();
				xr_string OutPath = CurrentWD + FileName;

				// Make temp file
				std::filesystem::copy_file(File.c_str(), OutPath.c_str());
				FS.TryLoad(OutPath.c_str());

				pObject->m_SMotionRefs.emplace_back(FileName.substr(0, FileName.length() - 4).c_str());
			}
			ExecCommand(COMMAND_UPDATE_PROPERTIES);
		}
		else
		{
			ErrorMsg();
		}
	}
}
} // namespace

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

	CoInitialize(nullptr);

	Core._initialize("Actor", ELogCallback, true, fsgame[0] ? fsgame : FSName);

	// Infernis: unattended checks (-frames, -shot, -save; see EditorCommandLine.h), before the tools and the main
	// form are created, so that dialogs during start-up are covered.
	CEditorCheckRun CheckRun;
	CheckRun.Parse(pCmdLine);

	psDeviceFlags.set(rsFullscreen, false);

	Tools = new CActorTools();
	ATools = (CActorTools*)Tools;

	UI = new CActorMain();
	UI->RegisterCommands();

	UIMainForm* MainForm = new UIMainForm();
	::MainForm = MainForm;

	PGMLib->Load();

	UI->PushBegin(MainForm, false);

	// Infernis: -smooth legacy|edges|normals picks how vertex normals are made, before an object is loaded (as the
	// Smooth buttons of the left bar do): legacy is the 3ds Max smoothing groups Shadow of Chornobyl's SDK used,
	// edges is Call of Pripyat's smoothing by marked edges (the default), normals takes the normals stored in the
	// object.
	string32 Smooth = "";
	if (ReadCommandLineValue(pCmdLine, "-smooth ", Smooth))
	{
		if (0 == xr_strcmp(Smooth, "legacy"))
			EPrefs->SmoothGroup = ESmoothGroup::Other;
		else if (0 == xr_strcmp(Smooth, "edges"))
			EPrefs->SmoothGroup = ESmoothGroup::Edges;
		else if (0 == xr_strcmp(Smooth, "normals"))
			EPrefs->SmoothGroup = ESmoothGroup::Normals;
		else
			Msg("! -smooth %s: expected legacy, edges or normals", Smooth);
	}

	// An object named on the command line (the first argument, as when a file is opened with the editor).
	int ArgsCount = 0;
	auto Commands = CommandLineToArgvW(GetCommandLineW(), &ArgsCount);
	if (ArgsCount > 1)
	{
		xr_string SecondArg = Platform::WideToCP1251(Commands[1]);
		if (SecondArg.ends_with(".object"))
			ExecCommand(COMMAND_LOAD, SecondArg);
	}
	LocalFree(Commands);

	// Infernis: export checks. With -frames, -export_ogf <file> and -export_omf <file> export the loaded object on
	// the last frame, as File > Export OGF / OMF do (OGF version 4, the motions' own form).
	string_path ExportOGF = "", ExportOMF = "";
	ReadCommandLineValue(pCmdLine, "-export_ogf ", ExportOGF);
	ReadCommandLineValue(pCmdLine, "-export_omf ", ExportOMF);

	bool NeedExit = false;
	while (!NeedExit && !UI->NeedQuit())
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
				if (UI)UI->KeyUp(Event.key.scancode, UI->GetShiftState());
				break;

			case SDL_EVENT_DROP_FILE:
			{
				xr_string File = Event.drop.data;
				File = xr_strlwr(File.data()); // IX-Ray lowercases dropped paths
				DragFile(File);
				break;
			}

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
			NeedExit = true;
			if (ExportOGF[0])
			{
				const bool Exported = ATools->ExportOGF(ExportOGF);
				Msg("* -export_ogf %s: %s", ExportOGF, Exported ? "exported" : "FAILED");
			}
			if (ExportOMF[0])
			{
				const bool Exported = ATools->ExportOMF(ExportOMF);
				Msg("* -export_omf %s: %s", ExportOMF, Exported ? "exported" : "FAILED");
			}
		}
	}
	xr_delete(MainForm);
	Core._destroy();
	return 0;
}
