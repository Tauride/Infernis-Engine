#pragma once

#define VPUSH(a) ((a).x), ((a).y), ((a).z)

void XRCORE_API Log(LPCSTR msg);
void XRCORE_API Log(LPCSTR msg);
void XRCORE_API Log(LPCSTR msg, LPCSTR dop);
void XRCORE_API Log(LPCSTR msg, u32 dop);
void XRCORE_API Log(LPCSTR msg, const u64 dop);
void XRCORE_API Log(LPCSTR msg, int dop);
void XRCORE_API Log(LPCSTR msg, float dop);
void XRCORE_API Log(LPCSTR msg, const Fvector& dop);
void XRCORE_API Log(LPCSTR msg, const Fmatrix& dop);
void XRCORE_API LogWinErr(LPCSTR msg, long err_code);

template <typename... Args>
[[deprecated]]
void Msg(const char* format, const Args&... args) {
    string2048 buf;
    const int sz = snprintf(buf, sizeof(buf) - 1, format, args...);
    buf[sizeof(buf) - 1] = 0;
    if (sz > 0)
        Log(buf);
}

template <typename... Args>
void LogMsg(const char* format, const Args&... args) {
    XrWriterAs<string2048> writer;
    writer.write(format, args...);
    if (writer.size() > 0)
        Log(writer.c_str()); // TODO: [imdex] use string_view
}

typedef void (*LogCallback)(LPCSTR string);
LogCallback XRCORE_API SetLogCB(LogCallback cb);
// A second, independent callback for every log line (SetLogCB's slot is taken by the
// on-screen error text). Called under the log lock, from whichever thread logs.
void XRCORE_API SetLogObserver(LogCallback cb);
XRCORE_API LPCSTR log_path(); // the log file's path as it was opened
void XRCORE_API CreateLog(BOOL no_log = FALSE);
void InitLog();
void CloseLog();
void XRCORE_API FlushLog();

extern XRCORE_API xr_vector<std::string>* LogFile;
extern XRCORE_API BOOL LogExecCB;
