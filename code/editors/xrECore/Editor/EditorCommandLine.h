#pragma once
// Reads the value after a command-line option ("-shot <file>", "-ogf <model>", "-fsltx <file>"): up to the next
// space, or between double quotes for values with spaces. Truncated to fit Out. Returns false when the option is not
// there.
inline bool ReadCommandLineValue(const char* CmdLine, const char* Option, char* Out, size_t OutSize)
{
    if (!OutSize)
        return false;
    Out[0] = 0;
    const char* Found = CmdLine ? strstr(CmdLine, Option) : nullptr;
    if (!Found)
        return false;
    const char* Value = Found + xr_strlen(Option);
    while (*Value == ' ')
        ++Value;
    const char End = (*Value == '"') ? '"' : ' ';
    if (End == '"')
        ++Value;
    size_t Length = 0;
    while (Value[Length] && Value[Length] != End)
        ++Length;
    if (Length >= OutSize)
        Length = OutSize - 1;
    std::memcpy(Out, Value, Length);
    Out[Length] = 0;
    return Length != 0;
}

template <size_t Size>
bool ReadCommandLineValue(const char* CmdLine, const char* Option, char (&Out)[Size])
{
    return ReadCommandLineValue(CmdLine, Option, Out, Size);
}

// Unattended checks shared by the editors' main loops (Infernis):
//   -frames <n>  close after n frames (drawn even with the pointer outside the window);
//   -shot <file> save the last frame (.png or .bmp);
//   -save        run the Save command on the last frame, without edits (open/save round trips).
// A timed run shows no dialogs: they are logged and answered OK, or Cancel/No for questions (CLog::Unattended).
struct CEditorCheckRun
{
    int FrameLimit = 0;
    int FrameCount = 0;
    string_path ShotPath = "";
    bool SaveAtEnd = false;

    void Parse(const char* CmdLine)
    {
        string32 Frames = "";
        if (ReadCommandLineValue(CmdLine, "-frames ", Frames))
            FrameLimit = atoi(Frames);
        ReadCommandLineValue(CmdLine, "-shot ", ShotPath);
        SaveAtEnd = FrameLimit > 0 && strstr(CmdLine, "-save") != nullptr;
        ELog.Unattended = FrameLimit > 0; // dialogs are logged and answered OK/Cancel (ELog.h)
        if (FrameLimit > 0)
            psDeviceFlags.set(rsRenderRealTime, TRUE); // redraw every frame (the editors redraw on request otherwise)
    }

    // Before each frame.
    void BeforeFrame()
    {
        if (FrameLimit <= 0)
            return;
        Device.b_is_Active = true; // the editors only draw while active
        if (ShotPath[0] && FrameCount == FrameLimit - 1)
            EDevice->RequestScreenshot(ShotPath);
    }

    // After each frame; true when the run is over.
    bool AfterFrame()
    {
        if (FrameLimit <= 0 || ++FrameCount < FrameLimit)
            return false;
        if (SaveAtEnd)
            Msg("* -save: %s", ExecCommand(COMMAND_SAVE) ? "saved" : "FAILED");
        if (EDevice->m_ScreenshotPath[0])
            Msg("! screenshot %s: not saved (no frame was drawn)", EDevice->m_ScreenshotPath);
        return true;
    }
};
