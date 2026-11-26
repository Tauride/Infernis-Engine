//---------------------------------------------------------------------------
// Sound environments (senvironment.xr): Infernis keeps GSC's EAX layout (CSoundRender_Environment), which the game's
// OpenAL EFX reverb reads (SoundRender_CoreA.cpp). Ported from IX-Ray's page, which edits IX-Ray's own EFX layout.
#ifndef SHSoundEnvToolsH
#define SHSoundEnvToolsH
#include "SHToolsInterface.h"
#include "../../engine/xrSound/SoundRender_Environment.h"

// refs
class PropValue;

class CSHSoundEnvTools: public ISHTools
{
    CSoundRender_Environment*	FindItem			(const char* name);

    SoundEnvironment_LIB		m_Library;

    shared_str					m_SoundName;
    CSound_params				m_Params;
    ref_sound					m_PreviewSnd;
	void   			OnControlClick		(ButtonValue* sender, bool& bModif, bool& bSafe);
	void   			OnChangeWAV			(PropValue* prop);

    void   			OnRevResetClick		(ButtonValue* sender, bool& bModif, bool& bSafe);
	void   			OnEnvChange			(PropValue* sender);
	bool			IsPreviewPlaying	() { return m_PreviewSnd._feedback() != nullptr; }
public:
    CSoundRender_Environment* 	m_Env;
    virtual void				AppendItem			(const char* folder_name, const char* parent=0);
    virtual void   	OnRemoveItem		(UIItemListForm::Node& node);
	virtual void   	OnRenameItem		(UIItemListForm::Node& node, const char* old_full_name, const char* new_full_name, EItemType type);
	virtual void 				FillItemList		();

    void						UseEnvironment		();
public:
								CSHSoundEnvTools 	(const ISHInit& init);
    virtual 					~CSHSoundEnvTools	();

    virtual const char*			ToolsName			(){return "Sound Environment";}

	virtual void				Reload				();
	virtual void				Load				();
	virtual bool				Save				();

    virtual bool				OnCreate			();
    virtual void				OnDestroy			();
	virtual void 				OnActivate			();
	virtual void 				OnDeactivate		();

    // misc
    virtual void				ResetCurrentItem	();
    virtual void				SetCurrentItem		(const char* name, bool bView);
    virtual void				ApplyChanges		(bool bForced=false);

	virtual void 				RealUpdateProperties();
	virtual void 				RealUpdateList		();

	virtual void 				OnFrame				();
	virtual void 				OnRender			();

    virtual void				OnDeviceCreate		(){;}
    virtual void				OnDeviceDestroy		(){;}
};
//---------------------------------------------------------------------------

#endif
