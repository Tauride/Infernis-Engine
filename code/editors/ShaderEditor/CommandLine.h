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
