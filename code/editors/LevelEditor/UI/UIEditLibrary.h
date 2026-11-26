#pragma once

class UIEditLibrary :
	public IEditorWnd
{
public:
	UIEditLibrary();
	virtual ~UIEditLibrary();

	static void Update();
	static void Show();
	static void Close();
	static void OnRender();
	// Infernis: Make LOD (high quality) for these objects (library names), with the preview on, for the unattended
	// checks (-make_lods)
	static void MakeLODs(const SharedStrVec& Objects);
	// Infernis: Make LOD runs in steps across frames, with a progress window on top: the editor's main loop calls
	// this each iteration (between frames)
	static void LODJobUpdate();
	static void StartLODs(const SharedStrVec& Objects); // opens the library and starts the job (high quality)
	static bool LODJobRunning();
	static void LODJobStop(); // cancels a running job and waits for its worker (before the editor shuts down)

	ref_texture m_RealTexture;

	void OnItemFocused(ListItem* item);
	void OnItemUnfocused(ListItem* item);

private:
	static UIEditLibrary* Form;
	ListItemsVec FocusedItems;

	virtual void Draw();
	void ImportClick();
	void DrawObjects();

	void DrawRightBar();
	void RenderSaveButton();
	void InitObjects();
	void OnPropertiesClick();
	void OnMakeThmClick();
	void OnPreviewClick();

	void MakeLOD(bool highQuality);
	void GenerateLOD(const SharedStrVec& props, bool bHighQuality); // starts the job (LODJobUpdate)
	void GenerateLODNow(const SharedStrVec& props, bool bHighQuality); // on this thread, the editor waiting

	void RefreshSelected();
	void PickSurface();
	void PickCallback();
	void ChangeReference(const SharedStrVec& items);
	bool SelectionToReference(ListItemsVec* props);
	void ExportOneOBJ(CEditableObject* EO);
	void ExportObj();

	UIPropertiesForm* GetPropertyWnd();

	static void OnModified();
	UIItemListForm& ActualItemList() { if (SearchQuery.empty()) return *m_ObjectList; return SearchList; }
	UIItemListForm* m_ObjectList;
	UIItemListForm SearchList;
	const char* m_Current;
	bool m_Preview;
	bool m_Dropper;

	xr_string PrevClick;

	UIRenderForm View;

	bool m_SelectLods;
	bool m_HighQualityLod;
	bool bShowProps = false;

	bool IsModify = false;
	bool UseWorldPropWnd = false;

	xr_vector<CSceneObject*> m_pEditObjects;
	UIPropertiesForm* PreviewProps = nullptr;
	UIPropertiesForm* InternalProps;
	xr_string SearchQuery;
};