//----------------------------------------------------
// Ported from IX-Ray xrCore/FileSystem.cpp (EFS_Utils -> CEditorFS)
//----------------------------------------------------

#include "stdafx.h"
#include "EditorFileSystem.h"
#include <commdlg.h>
#include <cderr.h>


CEditorFS EditorFS;
//----------------------------------------------------
xr_string	CEditorFS::ExtractFileName(const char* src)
{
	string_path name;
	_splitpath(src, nullptr, nullptr, name, nullptr);
	return xr_string(name);
}

xr_string	CEditorFS::ExtractFileExt(const char* src)
{
	string_path ext;
	_splitpath(src, nullptr, nullptr, nullptr, ext);
	return xr_string(ext);
}

xr_string	CEditorFS::ExtractFilePath(const char* src)
{
	string_path drive, dir;
	_splitpath(src, drive, dir, nullptr, nullptr);
	return xr_string(drive) + dir;
}

xr_string	CEditorFS::ExcludeBasePath(const char* full_path, const char* excl_path)
{
	const char* sub = strstr(full_path, excl_path);
	if (nullptr != sub) 	return xr_string(sub + xr_strlen(excl_path));
	else	   		return xr_string(full_path);
}

xr_string	CEditorFS::ChangeFileExt(const char* src, const char* ext)
{
	xr_string	tmp;
	LPSTR src_ext = strext(src);
	if (src_ext) {
		size_t		ext_pos = src_ext - src;
		tmp.assign(src, 0, ext_pos);
	}
	else {
		tmp = src;
	}
	tmp += ext;
	return tmp;
}

xr_string	CEditorFS::ChangeFileExt(const xr_string& src, const char* ext)
{
	return ChangeFileExt(src.c_str(), ext);
}

//----------------------------------------------------
static void MakeFilter(string1024& dest, const char* info, const char* ext)
{
	std::string res;

	if (ext)
	{
		res += info;
		res += "(";
		res += ext;
		res += ")|";
		res += ext;
		res += "|";
		int icnt = _GetItemCount(ext, ';');
		if (icnt > 1)
		{
			for (int idx = 0; idx < icnt; ++idx)
			{
				string64		buf;
				_GetItem(ext, idx, buf, ';');

				res += info;
				res += "(";
				res += buf;
				res += ")|";
				res += buf;
				res += "|";
			}
		}
		res += "|";
	}
	else
	{
		res = "All files(*.*)|*.*||";
	}
	xr_strcpy(dest, res.c_str());

	for (u32 i = 0; i < res.size(); ++i)
	{
		if (res[i] == '|')
			dest[i] = '\0';
	}


}

//------------------------------------------------------------------------------
// start_flt_ext = -1-all 0..n-indices
//------------------------------------------------------------------------------

// Vista uses this hook for old-style save dialog
static UINT_PTR CALLBACK OFNHookProcOldStyle(HWND, UINT, WPARAM, LPARAM)
{
	// let default hook work on this message
	return 0;
}

bool CEditorFS::GetOpenNameInternal(const char* initial, LPSTR buffer, int sz_buf, bool bMulti, const char* offset, int start_flt_ext, const char* ext)
{
#ifdef IXR_WINDOWS
	VERIFY(buffer && (sz_buf > 0));
	FS_Path& P = GetPathOrRoot(initial);
	string1024 			flt;
	MakeFilter(flt, "", ext ? ext : P.m_DefExt);

	OPENFILENAMEA 		ofn;
	memset(&ofn, 0, sizeof(ofn));

	if (xr_strlen(buffer))
	{
		string_path		dr;
		if (!(buffer[0] == '\\' && buffer[1] == '\\')) { // if !network
			_splitpath(buffer, dr, nullptr, nullptr, nullptr);

			if (0 == dr[0])
			{
				string_path		bb;
				P._update(bb, buffer);
				xr_strcpy(buffer, sz_buf, bb);
			}
		}
	}
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = GetForegroundWindow();
	ofn.lpstrDefExt = P.m_DefExt;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = sz_buf;
	ofn.lpstrFilter = flt;
	ofn.nFilterIndex = start_flt_ext + 2;
	ofn.lpstrTitle = "Open a File";
	string512 path;
	xr_strcpy(path, (offset && offset[0]) ? offset : P.m_Path);
	ofn.lpstrInitialDir = path;

	ofn.Flags = OFN_PATHMUSTEXIST |
		OFN_FILEMUSTEXIST |
		OFN_HIDEREADONLY |
		OFN_FILEMUSTEXIST |
		OFN_NOCHANGEDIR |
		(bMulti ? OFN_ALLOWMULTISELECT | OFN_EXPLORER : 0);

	ofn.FlagsEx = 0;

	bool bRes = !!GetOpenFileNameA(&ofn);
	if (!bRes)
	{
		u32 err = CommDlgExtendedError();
		switch (err)
		{
		case FNERR_BUFFERTOOSMALL:
			Log("Too many files selected.");
			break;
		}
	}
	if (bRes && bMulti)
	{
		int cnt = _GetItemCount(ofn.lpstrFile, 0x0);
		if (cnt > 1)
		{
			char 		dir[255 * 255];
			char 		buf[255 * 255];
			char 		fns[255 * 255];

			xr_strcpy(dir, buffer);
			xr_strcpy(fns, dir);
			xr_strcat(fns, "\\");
			xr_strcat(fns, _GetItem(ofn.lpstrFile, 1, buf, '\0'));

			for (int i = 2; i < cnt; i++)
			{
				xr_strcat(fns, ",");
				xr_strcat(fns, dir);
				xr_strcat(fns, "\\");
				xr_strcat(fns, _GetItem(ofn.lpstrFile, i, buf, '\0'));
			}
			xr_strcpy(buffer, sz_buf, fns);
		}
	}
	strlwr(buffer);
	return bRes;
#else
	return false;
#endif
}


bool CEditorFS::GetOpenName(const char* initial, xr_string& buffer, bool bMulti, const char* offset, int start_flt_ext, const char* ext)
{
	char			buf[255 * 255]; //max files to select
	xr_strcpy(buf, buffer.c_str());

	bool bRes = GetOpenNameInternal(initial, buf, sizeof(buf), bMulti, offset, start_flt_ext, ext);

	if (bRes)
		buffer = (char*)buf;

	return bRes;
}

bool CEditorFS::GetSaveName(const char* initial, string_path& buffer, const char* offset, int start_flt_ext, const char* ext)
{
#ifdef IXR_WINDOWS
	FS_Path& P = GetPathOrRoot(initial);
	string1024 			flt;

	const char* def_ext = P.m_DefExt;

	MakeFilter(flt, "", ext ? ext : def_ext);
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	if (xr_strlen(buffer)) {
		string_path		dr;
		if (!(buffer[0] == '\\' && buffer[1] == '\\')) { // if !network
			_splitpath(buffer, dr, nullptr, nullptr, nullptr);
			if (0 == dr[0])	P._update(buffer, buffer);
		}
	}
	ofn.hwndOwner = GetForegroundWindow();
	ofn.lpstrDefExt = def_ext;
	ofn.lpstrFile = buffer;
	ofn.lpstrFilter = flt;
	ofn.lStructSize = sizeof(ofn);
	ofn.nMaxFile = sizeof(buffer);
	ofn.nFilterIndex = start_flt_ext + 2;
	ofn.lpstrTitle = "Save a File";
	string512 path; xr_strcpy(path, (offset && offset[0]) ? offset : P.m_Path);
	ofn.lpstrInitialDir = path;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;
	ofn.FlagsEx = 0;


	bool bRes = !!GetSaveFileNameA(&ofn);
	if (!bRes) {
		u32 err = CommDlgExtendedError();
		switch (err) {
		case FNERR_BUFFERTOOSMALL: 	Log("Too many file selected."); break;
		}
	}
	strlwr(buffer);
	return bRes;
#else
	return false;
#endif
}


bool CEditorFS::GetSaveName(const char* initial, xr_string& buffer, const char* offset, int start_flt_ext, const char* ext)
{
	string_path				buf;
	xr_strcpy(buf, sizeof(buf), buffer.c_str());
	bool bRes = GetSaveName(initial, buf, offset, start_flt_ext, ext);
	if (bRes)
		buffer = buf;

	return bRes;
}

xr_string CEditorFS::AppendFolderToName(xr_string& tex_name, int depth, bool full_name)
{
	string1024 nm;
	xr_strcpy(nm, tex_name.c_str());
	tex_name = AppendFolderToName(nm, sizeof(nm), depth, full_name);
	return tex_name;
}

void CEditorFS::MarkFile(const char* fn, bool bDeleteSource)
{
	xr_string ext = strext(fn);
	ext.insert(1, "~");
	xr_string backup_fn = EFS.ChangeFileExt(fn, ext.c_str());
	if (bDeleteSource) {
		FS.file_rename(fn, backup_fn.c_str(), true);
	}
	else {
		FS.file_copy(fn, backup_fn.c_str());
	}
}

//----------------------------------------------------
const char* CEditorFS::AppendFolderToName(LPSTR tex_name, u32 const tex_name_size, int depth, bool full_name)
{
	string256 _fn;
	xr_strcpy(tex_name, tex_name_size, AppendFolderToName(tex_name, _fn, sizeof(_fn), depth, full_name));
	return tex_name;
}

const char* CEditorFS::AppendFolderToName(const char* src_name, LPSTR dest_name, u32 const dest_name_size, int depth, bool full_name)
{
	shared_str tmp = src_name;
	const char* s = src_name;
	LPSTR d = dest_name;
	int sv_depth = depth;
	for (; *s && depth; s++, d++) {
		if (*s == '_') { depth--; *d = '\\'; }
		else { *d = *s; }
	}
	if (full_name) {
		*d = 0;
		if (depth < sv_depth)	xr_strcat(dest_name, dest_name_size, *tmp);
	}
	else {
		for (; *s; s++, d++) *d = *s;
		*d = 0;
	}
	return dest_name;
}

const char* CEditorFS::GenerateName(const char* base_path, const char* base_name, const char* def_ext, LPSTR out_name, u32 const out_name_size)
{
	int cnt = 0;
	string_path fn;
	if (base_name)
		strconcat(sizeof(fn), fn, base_path, base_name, def_ext);
	else
		xr_sprintf(fn, sizeof(fn), "%s%02d%s", base_path, cnt++, def_ext);

	while (FS.exist(fn))
		if (base_name)
			xr_sprintf(fn, sizeof(fn), "%s%s%02d%s", base_path, base_name, cnt++, def_ext);
		else
			xr_sprintf(fn, sizeof(fn), "%s%02d%s", base_path, cnt++, def_ext);
	xr_strcpy(out_name, out_name_size, fn);
	return out_name;
}

//#endif

//----------------------------------------------------
// Infernis additions
//----------------------------------------------------
bool CEditorFS::ReadDiskFile(const char* path, xr_string& data)
{
	FILE* File = fopen(path, "rb");
	if (!File)
		return false;
	fseek(File, 0, SEEK_END);
	const long Size = ftell(File);
	fseek(File, 0, SEEK_SET);
	data.assign(Size > 0 ? (size_t)Size : 0, '\0');
	const bool Ok = Size >= 0 && fread(data.data(), 1, data.size(), File) == data.size();
	fclose(File);
	return Ok;
}

FS_Path& CEditorFS::GetPathOrRoot(const char* initial)
{
	if (initial && FS.path_exist(initial))
		return *FS.get_path(initial);
	Msg("~ Editor: path alias %s is not in fsgame.ltx, using $fs_root$", initial ? initial : "(null)");
	return *FS.get_path("$fs_root$");
}

bool CEditorFS::TryLoad(const char* path)
{
	if (!path || !path[0])
		return false;
	if (FS.exist(path))
		return true;
	std::error_code Error;
	if (!std::filesystem::exists(path, Error))
		return false;
	string_path folder;
	xr_strcpy(folder, ExtractFilePath(path).c_str());
	FS.rescan_path(folder, FALSE);
	return FS.exist(path) != nullptr;
}

xr_string CEditorFS::FixPath(const char* path)
{
	if (!path)
		return xr_string();
	if (FS.path_exist("$game_data$"))
	{
		const char* root = FS.get_path("$game_data$")->m_Path;
		const size_t root_length = xr_strlen(root);
		if (root_length && 0 == _strnicmp(path, root, root_length))
			return xr_string(path + root_length);
	}
	return xr_string(path);
}
