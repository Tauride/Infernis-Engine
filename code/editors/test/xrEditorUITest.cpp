// Stage 1 check for the editor port: the IX-Ray editor shell (xrEUI) and a property grid (xrEProps) running on
// Infernis's xrCore. Opens an SDL3 window with a D3D9 device, docks a properties window and draws until closed.
//
// Run from the folder that holds fsgame.ltx (the repository root):
//   xrEditorUITest.exe [-frames <n>] [-shot <file.bmp>]
// -frames closes the window after n frames; -shot saves the last frame as a 32-bit BMP before closing.

#include "xrEProps/stdafx.h"
#include "Public/PropertiesListHelper.h"
#include <SDL3/SDL.h>
#include <d3d9.h>

namespace
{
class TestUIManager final : public XrUIManager
{
public:
    bool ApplyShortCut(DWORD, TShiftState) override { return false; }
};

class PropertiesWindow final : public IEditorWnd
{
public:
    PropertiesWindow()
    {
        PHelper().CreateCaption(Items, "Info\\Engine", "Infernis");
        PHelper().CreateFloat(Items, "Values\\Float", &FloatValue, 0.f, 10.f, 0.1f, 2);
        PHelper().CreateU32(Items, "Values\\U32", &U32Value, 0, 1000, 1);
        PHelper().CreateBool(Items, "Values\\Bool", &BoolValue);
        PHelper().CreateVector(Items, "Values\\Vector", &VectorValue, -100.f, 100.f, 0.1f, 2);
        PHelper().CreateColor(Items, "Values\\Color", &ColorValue);
        PHelper().CreateSText(Items, "Values\\Text", &TextValue);
        Form.AssignItems(Items);
    }

    void Draw() override
    {
        // Multi-viewport ImGui: positions are in screen space, so offset from the main window.
        const ImVec2 Origin = ImGui::GetMainViewport()->Pos;
        ImGui::SetNextWindowPos(ImVec2(Origin.x + 80, Origin.y + 120), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Properties", &bOpen))
            Form.Draw();
        ImGui::End();
    }

private:
    UIPropertiesForm Form;
    PropItemVec Items;
    float FloatValue = 2.5f;
    u32 U32Value = 42;
    bool BoolValue = true;
    Fvector VectorValue = {1.f, 2.f, 3.f};
    u32 ColorValue = 0xFF3080C0;
    xr_string TextValue = "Shadow of Chornobyl";
};

bool SaveBackBuffer(IDirect3DDevice9* Device, const char* Path)
{
    IDirect3DSurface9* BackBuffer = nullptr;
    if (FAILED(Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &BackBuffer)))
        return false;

    D3DSURFACE_DESC Desc;
    BackBuffer->GetDesc(&Desc);
    IDirect3DSurface9* Copy = nullptr;
    bool Saved = false;
    if (SUCCEEDED(Device->CreateOffscreenPlainSurface(Desc.Width, Desc.Height, Desc.Format, D3DPOOL_SYSTEMMEM, &Copy, nullptr)) &&
        SUCCEEDED(Device->GetRenderTargetData(BackBuffer, Copy)))
    {
        D3DLOCKED_RECT Locked;
        if (SUCCEEDED(Copy->LockRect(&Locked, nullptr, D3DLOCK_READONLY)))
        {
            if (FILE* File = fopen(Path, "wb"))
            {
                const u32 RowSize = Desc.Width * 4;
                BITMAPFILEHEADER FileHeader = {};
                BITMAPINFOHEADER InfoHeader = {};
                FileHeader.bfType = 0x4D42;
                FileHeader.bfOffBits = sizeof(FileHeader) + sizeof(InfoHeader);
                FileHeader.bfSize = FileHeader.bfOffBits + RowSize * Desc.Height;
                InfoHeader.biSize = sizeof(InfoHeader);
                InfoHeader.biWidth = (LONG)Desc.Width;
                InfoHeader.biHeight = -(LONG)Desc.Height; // top-down
                InfoHeader.biPlanes = 1;
                InfoHeader.biBitCount = 32;
                fwrite(&FileHeader, sizeof(FileHeader), 1, File);
                fwrite(&InfoHeader, sizeof(InfoHeader), 1, File);
                for (u32 Row = 0; Row < Desc.Height; ++Row)
                    fwrite((const u8*)Locked.pBits + Row * Locked.Pitch, RowSize, 1, File);
                fclose(File);
                Saved = true;
            }
            Copy->UnlockRect();
        }
    }
    if (Copy)
        Copy->Release();
    BackBuffer->Release();
    return Saved;
}
} // namespace

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR CmdLine, int)
{
    int MaxFrames = 0;
    string_path ShotPath = {};
    if (const char* Arg = strstr(CmdLine, "-frames "))
        MaxFrames = atoi(Arg + 8);
    if (const char* Arg = strstr(CmdLine, "-shot "))
        sscanf(Arg + 6, "%259s", ShotPath);

    Core._initialize("xrEditorUITest", nullptr, TRUE, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        Msg("! SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const int Width = 1280, Height = 720;
    SDL_Window* Window = SDL_CreateWindow("xrEditorUITest - Infernis editor shell", Width, Height, SDL_WINDOW_RESIZABLE);
    HWND Hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    g_AppInfo.Window = Window;

    IDirect3D9* D3D = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS Params = {};
    Params.Windowed = TRUE;
    Params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    Params.BackBufferFormat = D3DFMT_A8R8G8B8;
    Params.BackBufferWidth = Width;
    Params.BackBufferHeight = Height;
    Params.EnableAutoDepthStencil = TRUE;
    Params.AutoDepthStencilFormat = D3DFMT_D24S8;
    Params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    IDirect3DDevice9* Device = nullptr;
    if (!D3D || FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                         &Params, &Device)))
    {
        Msg("! Direct3D 9 device creation failed");
        return 1;
    }

    TestUIManager UI;
    GUIManager = &UI;
    string_path IniPath;
    FS.update_path(IniPath, "$app_data_root$", "xrEditorUITest_imgui.ini");
    UI.Initialize(Hwnd, Device, IniPath);
    UI.Push(new PropertiesWindow());

    bool Running = true;
    for (int Frame = 0; Running; ++Frame)
    {
        SDL_Event Event;
        while (SDL_PollEvent(&Event))
        {
            UI.ProcessEvent(&Event);
            if (Event.type == SDL_EVENT_QUIT || Event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                Running = false;
        }

        UI.BeginFrame();
        UI.Draw();
        Device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(30, 30, 30), 1.f, 0);
        Device->BeginScene();
        UI.EndFrame();
        Device->EndScene();

        const bool LastFrame = MaxFrames > 0 && Frame + 1 >= MaxFrames;
        if (LastFrame && ShotPath[0])
            Msg("* xrEditorUITest: screenshot %s: %s", ShotPath, SaveBackBuffer(Device, ShotPath) ? "saved" : "FAILED");
        Device->Present(nullptr, nullptr, nullptr, nullptr);
        UI.MDIUpdate();
        if (LastFrame)
            Running = false;
    }

    Msg("* xrEditorUITest: closed cleanly");
    UI.Destroy();
    Device->Release();
    D3D->Release();
    SDL_DestroyWindow(Window);
    SDL_Quit();
    Core._destroy();
    return 0;
}
