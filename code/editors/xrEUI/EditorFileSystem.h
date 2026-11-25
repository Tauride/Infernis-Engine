#pragma once
// Editor file helpers and Win32 open/save dialogs, ported from IX-Ray xrCore/FileSystem.* (EFS_Utils).
// Infernis's xrCore keeps only EFS_Utils::ChangeFileExt, so the editors own this class; editor code reaches it
// through the EFS macro as in IX-Ray.

class XREUI_API CEditorFS
{
public:
    const char* GenerateName(const char* base_path, const char* base_name, const char* def_ext, LPSTR out_name,
                             u32 const out_name_size);

    bool GetOpenName(const char* initial, xr_string& buf, bool bMulti = false, const char* offset = nullptr,
                     int start_flt_ext = -1, const char* ext = nullptr);
    bool GetSaveName(const char* initial, string_path& buffer, const char* offset = nullptr, int start_flt_ext = -1,
                     const char* ext = nullptr);
    bool GetSaveName(const char* initial, xr_string& buf, const char* offset = nullptr, int start_flt_ext = -1,
                     const char* ext = nullptr);

    void MarkFile(const char* fn, bool bDeleteSource);

    // Infernis: reads a file straight from disk, for paths the locator has not indexed (IX-Ray used
    // FS.TryLoad to register them first). False when the file cannot be opened.
    bool ReadDiskFile(const char* path, xr_string& data);

    // Infernis: the path alias if fsgame.ltx defines it, otherwise $fs_root$. IX-Ray's fs.ltx defines aliases
    // such as $import$ that Infernis's does not, and FS.get_path asserts on unknown aliases.
    FS_Path& GetPathOrRoot(const char* initial);

    xr_string AppendFolderToName(xr_string& tex_name, int depth, bool full_name);
    const char* AppendFolderToName(LPSTR tex_name, u32 const tex_name_size, int depth, bool full_name);
    const char* AppendFolderToName(const char* src_name, LPSTR dest_name, u32 const dest_name_size, int depth,
                                   bool full_name);

    xr_string ChangeFileExt(const char* src, const char* ext);
    xr_string ChangeFileExt(const xr_string& src, const char* ext);

    xr_string ExtractFileName(const char* src);
    xr_string ExtractFilePath(const char* src);
    xr_string ExtractFileExt(const char* src);
    xr_string ExcludeBasePath(const char* full_path, const char* excl_path);

private:
    bool GetOpenNameInternal(const char* initial, LPSTR buffer, int sz_buf, bool bMulti, const char* offset,
                             int start_flt_ext, const char* ext);
};

extern XREUI_API CEditorFS EditorFS;
#define EFS EditorFS
