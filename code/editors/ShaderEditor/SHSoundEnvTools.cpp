//---------------------------------------------------------------------------
// Sound environment page. The structure (list, properties, preview sound) is IX-Ray's; the data is Infernis's EAX
// layout. Levels are stored in millibels, which the game turns into EFX gains with 10^(mB/2000)
// (SoundRender_CoreA.cpp), so the presets are OpenAL's EFX presets with their gains converted back.
#include "stdafx.h"

#include "ui_shadermain.h"
#include <mmsystem.h> // WAVEFORMATEX for the sound source header (xrSound's stdafx.h has it)
#include <mmreg.h>
#include "../../engine/xrSound/SoundRender_Source.h"
#include "../xrECore/Editor/D3DUtils.h"
#include "openal/efx-presets.h"

#include "SHSoundEnvTools.h"

namespace
{
// EAX 2 environments: the index is what CSoundRender_Environment::Environment stores.
enum
{
    EAX_ENVIRONMENT_GENERIC,
    EAX_ENVIRONMENT_PADDEDCELL,
    EAX_ENVIRONMENT_ROOM,
    EAX_ENVIRONMENT_BATHROOM,
    EAX_ENVIRONMENT_LIVINGROOM,
    EAX_ENVIRONMENT_STONEROOM,
    EAX_ENVIRONMENT_AUDITORIUM,
    EAX_ENVIRONMENT_CONCERTHALL,
    EAX_ENVIRONMENT_CAVE,
    EAX_ENVIRONMENT_ARENA,
    EAX_ENVIRONMENT_HANGAR,
    EAX_ENVIRONMENT_CARPETEDHALLWAY,
    EAX_ENVIRONMENT_HALLWAY,
    EAX_ENVIRONMENT_STONECORRIDOR,
    EAX_ENVIRONMENT_ALLEY,
    EAX_ENVIRONMENT_FOREST,
    EAX_ENVIRONMENT_CITY,
    EAX_ENVIRONMENT_MOUNTAINS,
    EAX_ENVIRONMENT_QUARRY,
    EAX_ENVIRONMENT_PLAIN,
    EAX_ENVIRONMENT_PARKINGLOT,
    EAX_ENVIRONMENT_SEWERPIPE,
    EAX_ENVIRONMENT_UNDERWATER,
    EAX_ENVIRONMENT_DRUGGED,
    EAX_ENVIRONMENT_DIZZY,
    EAX_ENVIRONMENT_PSYCHOTIC,
    EAX_ENVIRONMENT_COUNT
};

xr_token eax_environment[] = {
    {"Alley", EAX_ENVIRONMENT_ALLEY},
    {"Arena", EAX_ENVIRONMENT_ARENA},
    {"Auditorium", EAX_ENVIRONMENT_AUDITORIUM},
    {"Bathroom", EAX_ENVIRONMENT_BATHROOM},
    {"Carpeted Hallway", EAX_ENVIRONMENT_CARPETEDHALLWAY},
    {"Cave", EAX_ENVIRONMENT_CAVE},
    {"City", EAX_ENVIRONMENT_CITY},
    {"Concert Hall", EAX_ENVIRONMENT_CONCERTHALL},
    {"Dizzy", EAX_ENVIRONMENT_DIZZY},
    {"Drugged", EAX_ENVIRONMENT_DRUGGED},
    {"Forest", EAX_ENVIRONMENT_FOREST},
    {"Generic", EAX_ENVIRONMENT_GENERIC},
    {"Hallway", EAX_ENVIRONMENT_HALLWAY},
    {"Hangar", EAX_ENVIRONMENT_HANGAR},
    {"Living Room", EAX_ENVIRONMENT_LIVINGROOM},
    {"Mountains", EAX_ENVIRONMENT_MOUNTAINS},
    {"Padded Cell", EAX_ENVIRONMENT_PADDEDCELL},
    {"Parking Lot", EAX_ENVIRONMENT_PARKINGLOT},
    {"Plain", EAX_ENVIRONMENT_PLAIN},
    {"Psychotic", EAX_ENVIRONMENT_PSYCHOTIC},
    {"Quarry", EAX_ENVIRONMENT_QUARRY},
    {"Room", EAX_ENVIRONMENT_ROOM},
    {"Sewer Pipe", EAX_ENVIRONMENT_SEWERPIPE},
    {"Stone Corridor", EAX_ENVIRONMENT_STONECORRIDOR},
    {"Stone Room", EAX_ENVIRONMENT_STONEROOM},
    {"Under Water", EAX_ENVIRONMENT_UNDERWATER},
    {nullptr, 0}};

// OpenAL's presets for the EAX environments, in EAX order.
const EFXEAXREVERBPROPERTIES EnvironmentPresets[EAX_ENVIRONMENT_COUNT] = {
    EFX_REVERB_PRESET_GENERIC,         EFX_REVERB_PRESET_PADDEDCELL,  EFX_REVERB_PRESET_ROOM,
    EFX_REVERB_PRESET_BATHROOM,        EFX_REVERB_PRESET_LIVINGROOM,  EFX_REVERB_PRESET_STONEROOM,
    EFX_REVERB_PRESET_AUDITORIUM,      EFX_REVERB_PRESET_CONCERTHALL, EFX_REVERB_PRESET_CAVE,
    EFX_REVERB_PRESET_ARENA,           EFX_REVERB_PRESET_HANGAR,      EFX_REVERB_PRESET_CARPETEDHALLWAY,
    EFX_REVERB_PRESET_HALLWAY,         EFX_REVERB_PRESET_STONECORRIDOR, EFX_REVERB_PRESET_ALLEY,
    EFX_REVERB_PRESET_FOREST,          EFX_REVERB_PRESET_CITY,        EFX_REVERB_PRESET_MOUNTAINS,
    EFX_REVERB_PRESET_QUARRY,          EFX_REVERB_PRESET_PLAIN,       EFX_REVERB_PRESET_PARKINGLOT,
    EFX_REVERB_PRESET_SEWERPIPE,       EFX_REVERB_PRESET_UNDERWATER,  EFX_REVERB_PRESET_DRUGGED,
    EFX_REVERB_PRESET_DIZZY,           EFX_REVERB_PRESET_PSYCHOTIC,
};

// A linear EFX gain as the millibels the environment stores (inverse of the game's 10^(mB/2000)).
float GainToMillibels(float Gain)
{
    return Gain > 0.f ? 2000.f * log10f(Gain) : -10000.f;
}

// EAX 2 listener limits (eax.h), as GSC's page used them.
constexpr float EAXLISTENER_MINENVIRONMENTSIZE = 1.0f, EAXLISTENER_MAXENVIRONMENTSIZE = 100.0f;
constexpr float EAXLISTENER_MINENVIRONMENTDIFFUSION = 0.0f, EAXLISTENER_MAXENVIRONMENTDIFFUSION = 1.0f;
constexpr float EAXLISTENER_MINROOM = -10000.f, EAXLISTENER_MAXROOM = 0.f;
constexpr float EAXLISTENER_MINROOMHF = -10000.f, EAXLISTENER_MAXROOMHF = 0.f;
constexpr float EAXLISTENER_MINROOMROLLOFFFACTOR = 0.0f, EAXLISTENER_MAXROOMROLLOFFFACTOR = 10.0f;
constexpr float EAXLISTENER_MINAIRABSORPTIONHF = -100.0f, EAXLISTENER_MAXAIRABSORPTIONHF = 0.0f;
constexpr float EAXLISTENER_MINREFLECTIONS = -10000.f, EAXLISTENER_MAXREFLECTIONS = 1000.f;
constexpr float EAXLISTENER_MINREFLECTIONSDELAY = 0.0f, EAXLISTENER_MAXREFLECTIONSDELAY = 0.3f;
constexpr float EAXLISTENER_MINREVERB = -10000.f, EAXLISTENER_MAXREVERB = 2000.f;
constexpr float EAXLISTENER_MINREVERBDELAY = 0.0f, EAXLISTENER_MAXREVERBDELAY = 0.1f;
constexpr float EAXLISTENER_MINDECAYTIME = 0.1f, EAXLISTENER_MAXDECAYTIME = 20.0f;
constexpr float EAXLISTENER_MINDECAYHFRATIO = 0.1f, EAXLISTENER_MAXDECAYHFRATIO = 2.0f;
} // namespace

//------------------------------------------------------------------------------
CSHSoundEnvTools::CSHSoundEnvTools(const ISHInit& init) : ISHTools(init)
{
    m_Env = nullptr;
    // Infernis: no default preview sound. IX-Ray's (alexmx\beep) is an SDK test sound the game does not ship, and
    // the sound loader stops on a missing file. Pick one under Source in the preview properties.
}

CSHSoundEnvTools::~CSHSoundEnvTools() {}
//---------------------------------------------------------------------------

// Previews the current environment through the sound manager's EFX override (the game's script hook). GSC's
// editors called set_user_env, which exists only in an _EDITOR build of xrSound; the game's xrSound.dll is built
// without it, and the core's class differs between the two builds, so the editor stays on the shared interface.
void CSHSoundEnvTools::UseEnvironment()
{
    if (!Sound)
        return;
    if (!m_Env)
    {
        Sound->set_efx_override(false);
        return;
    }
    const CSoundRender_Environment& E = *m_Env;
    Sound->set_efx_override(true, E.Room, E.RoomHF, E.DecayTime, E.DecayHFRatio, E.ReflectionsDelay, E.ReverbDelay,
                            E.RoomRolloffFactor, E.EnvironmentDiffusion, E.Reflections, E.Reverb, E.AirAbsorptionHF);
}

void CSHSoundEnvTools::OnChangeWAV(PropValue*)
{
    const bool bPlay = IsPreviewPlaying();
    m_PreviewSnd.destroy();
    string_path File;
    if (m_SoundName.size() && !FS.exist(File, "$game_sounds$", *m_SoundName, ".ogg"))
    {
        ELog.DlgMsg(mtError, "Sound '%s' not found.", *m_SoundName);
        m_SoundName = "";
    }
    if (m_SoundName.size())
    {
        m_PreviewSnd.create(*m_SoundName, st_Effect, sg_Undefined);
        if (CSoundRender_Source* Source = static_cast<CSoundRender_Source*>(m_PreviewSnd._handle()))
        {
            m_Params.min_distance = Source->m_fMinDist;
            m_Params.max_distance = Source->m_fMaxDist;
        }
    }
    if (bPlay)
        m_PreviewSnd.play(nullptr, sm_Looped);
}

void CSHSoundEnvTools::OnControlClick(ButtonValue* V, bool& bModif, bool&)
{
    switch (V->btn_num)
    {
    case 0: m_PreviewSnd.play(nullptr, sm_Looped); break;
    case 1: m_PreviewSnd.stop(); break;
    }
    bModif = false;
}

void CSHSoundEnvTools::OnActivate()
{
    PropItemVec items;
    PropValue* V = PHelper().CreateChoose(items, "Source\\WAVE name", &m_SoundName, smSoundSource);
    V->OnChangeEvent.bind(this, &CSHSoundEnvTools::OnChangeWAV);
    ButtonValue* B = PHelper().CreateButton(items, "Source\\Controls", "Play,Stop", 0);
    B->OnBtnClickEvent.bind(this, &CSHSoundEnvTools::OnControlClick);
    Ext.m_PreviewProps->AssignItems(items);

    FillItemList();
    Ext.m_Items->SetOnItemCloneEvent({this, &CSHSoundEnvTools::OnCloneItem});
    Ext.m_Items->SetOnItemCreaetEvent({this, &CSHSoundEnvTools::OnCreateItem});
    Ext.m_Items->SetOnItemRenameEvent({this, &CSHSoundEnvTools::OnRenameItem});
    Ext.m_Items->SetOnItemRemoveEvent({this, &CSHSoundEnvTools::OnRemoveItem});
    inherited::OnActivate();
}
//---------------------------------------------------------------------------

void CSHSoundEnvTools::OnDeactivate()
{
    m_PreviewSnd.stop();
    inherited::OnDeactivate();
}
//---------------------------------------------------------------------------

void CSHSoundEnvTools::OnFrame() { inherited::OnFrame(); }
//---------------------------------------------------------------------------

#define SOUND_SEL0_COLOR 0x00A0A0F0
#define SOUND_SEL1_COLOR 0x00FFFFFF

void CSHSoundEnvTools::OnRender()
{
    if (IsPreviewPlaying())
    {
        RCache.set_xform_world(Fidentity);
        EDevice->SetShader(EDevice->m_WireShader);
        DU_impl.DrawLineSphere(Fvector().set(0, 0, 0), m_Params.max_distance, SOUND_SEL1_COLOR, true);
        DU_impl.DrawLineSphere(Fvector().set(0, 0, 0), m_Params.min_distance, SOUND_SEL0_COLOR, false);
    }
}
//---------------------------------------------------------------------------

bool CSHSoundEnvTools::OnCreate()
{
    Load();
    return true;
}

void CSHSoundEnvTools::OnDestroy()
{
    m_PreviewSnd.destroy();
    m_Library.Unload();
    m_bModified = false;
}
//---------------------------------------------------------------------------

void CSHSoundEnvTools::ApplyChanges(bool) { UseEnvironment(); }

void CSHSoundEnvTools::Reload()
{
    ResetCurrentItem();
    Load();
    FillItemList();
}

void CSHSoundEnvTools::FillItemList()
{
    ListItemsVec items;
    for (CSoundRender_Environment* E : m_Library.Library())
        LHelper().CreateItem(items, *E->name, 0);
    Ext.m_Items->AssignItems(items, 0, false);
}

void CSHSoundEnvTools::Load()
{
    string_path fn;
    FS.update_path(fn, _game_data_, SNDENV_FILENAME);

    m_bLockUpdate = true;
    if (FS.exist(fn))
    {
        m_Library.Unload();
        m_Library.Load(fn);
    }
    else
    {
        ELog.DlgMsg(mtInformation, "Can't find file '%s'", fn);
    }
    m_bLockUpdate = false;
}

bool CSHSoundEnvTools::Save()
{
    ApplyChanges();
    m_bLockUpdate = true;

    string_path fn;
    FS.update_path(fn, _game_data_, SNDENV_FILENAME);
    EFS.MarkFile(fn, false);
    const bool bRes = m_Library.Save(fn);
    m_bLockUpdate = false;

    if (bRes)
        m_bModified = false;
    return bRes;
}

CSoundRender_Environment* CSHSoundEnvTools::FindItem(const char* name)
{
    return (name && name[0]) ? m_Library.Get(name) : nullptr;
}

void CSHSoundEnvTools::AppendItem(const char* folder_name, const char* parent_name)
{
    CSoundRender_Environment* parent = FindItem(parent_name);
    m_LastSelection = folder_name;
    CSoundRender_Environment* S = m_Library.Append(parent);
    if (!parent)
        S->set_default();
    S->name = m_LastSelection.c_str();
    ExecCommand(COMMAND_UPDATE_LIST);
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
    Modified();
}

void CSHSoundEnvTools::OnRenameItem(UIItemListForm::Node&, const char* old_full_name, const char* new_full_name,
                                    EItemType type)
{
    if (type == TYPE_OBJECT)
    {
        ApplyChanges();
        CSoundRender_Environment* S = m_Library.Get(old_full_name);
        R_ASSERT(S);
        S->name = new_full_name;
        ExecCommand(COMMAND_UPDATE_PROPERTIES);
        ExecCommand(COMMAND_UPDATE_LIST);
    }
}

void CSHSoundEnvTools::OnRemoveItem(UIItemListForm::Node& node)
{
    string_path path;
    if (node.Path.size())
        xr_strconcat(path, node.Path.c_str(), "\\", node.Name.c_str());
    else
        xr_strcpy(path, node.Name.c_str());
    if (node.IsObject())
    {
        if (m_Env && m_Env->name == path)
        {
            m_Env = nullptr;
            UseEnvironment();
            Tools->UpdateProperties(true);
        }
        m_Library.Remove(path);
    }
}

void CSHSoundEnvTools::SetCurrentItem(const char* name, bool bView)
{
    if (m_bLockUpdate)
        return;
    CSoundRender_Environment* S = FindItem(name);
    if (m_Env != S)
    {
        m_Env = S;
        ExecCommand(COMMAND_UPDATE_PROPERTIES);
        if (bView)
            ViewSetCurrentItem(name);
    }
    UseEnvironment();
}

void CSHSoundEnvTools::ResetCurrentItem()
{
    m_Env = nullptr;
    UseEnvironment();
}

void CSHSoundEnvTools::OnRevResetClick(ButtonValue* V, bool&, bool&)
{
    switch (V->btn_num)
    {
    case 0: m_Env->set_identity(); break;
    case 1: OnEnvChange(V); break;
    }
    UseEnvironment();
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
    Modified();
}

// Sets the levels, times and shape of the chosen preset. The environment size is kept: GSC's page let EAX hardware
// rescale for it, and Infernis's reverb does not use it.
void CSHSoundEnvTools::OnEnvChange(PropValue*)
{
    if (!m_Env || m_Env->Environment >= EAX_ENVIRONMENT_COUNT)
        return;
    const EFXEAXREVERBPROPERTIES& P = EnvironmentPresets[m_Env->Environment];
    m_Env->Room = GainToMillibels(P.flGain);
    m_Env->RoomHF = GainToMillibels(P.flGainHF);
    m_Env->RoomRolloffFactor = P.flRoomRolloffFactor;
    m_Env->DecayTime = P.flDecayTime;
    m_Env->DecayHFRatio = P.flDecayHFRatio;
    m_Env->Reflections = GainToMillibels(P.flReflectionsGain);
    m_Env->ReflectionsDelay = P.flReflectionsDelay;
    m_Env->Reverb = GainToMillibels(P.flLateReverbGain);
    m_Env->ReverbDelay = P.flLateReverbDelay;
    m_Env->EnvironmentDiffusion = P.flDiffusion;
    m_Env->AirAbsorptionHF = GainToMillibels(P.flAirAbsorptionGainHF);
    m_Env->clamp();
    UseEnvironment();
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}

void CSHSoundEnvTools::RealUpdateList() { FillItemList(); }

void CSHSoundEnvTools::RealUpdateProperties()
{
    PropItemVec items;
    if (m_Env)
    {
        CSoundRender_Environment& S = *m_Env;
        ButtonValue* B = PHelper().CreateButton(items, "Environment\\Set", "Identity,Reset", ButtonValue::flFirstOnly);
        B->OnBtnClickEvent.bind(this, &CSHSoundEnvTools::OnRevResetClick);
        PropValue* V = PHelper().CreateToken32(items, "Environment\\Preset", &S.Environment, eax_environment);
        V->OnChangeEvent.bind(this, &CSHSoundEnvTools::OnEnvChange);
        PHelper().CreateFloat(items, "Environment\\Size", &S.EnvironmentSize, EAXLISTENER_MINENVIRONMENTSIZE,
                              EAXLISTENER_MAXENVIRONMENTSIZE, 0.01f, 3);
        PHelper().CreateFloat(items, "Environment\\Diffusion", &S.EnvironmentDiffusion,
                              EAXLISTENER_MINENVIRONMENTDIFFUSION, EAXLISTENER_MAXENVIRONMENTDIFFUSION, 0.01f, 3);
        PHelper().CreateFloat(items, "Room\\Room", &S.Room, EAXLISTENER_MINROOM, EAXLISTENER_MAXROOM, 1.f, 0);
        PHelper().CreateFloat(items, "Room\\Room HF", &S.RoomHF, EAXLISTENER_MINROOMHF, EAXLISTENER_MAXROOMHF, 1.f, 0);
        PHelper().CreateFloat(items, "Distance Effects\\Room Rolloff Factor", &S.RoomRolloffFactor,
                              EAXLISTENER_MINROOMROLLOFFFACTOR, EAXLISTENER_MAXROOMROLLOFFFACTOR, 0.01f, 3);
        PHelper().CreateFloat(items, "Distance Effects\\Air Absorption HF", &S.AirAbsorptionHF,
                              EAXLISTENER_MINAIRABSORPTIONHF, EAXLISTENER_MAXAIRABSORPTIONHF, 0.01f, 3);
        PHelper().CreateFloat(items, "Reflections\\Reflections", &S.Reflections, EAXLISTENER_MINREFLECTIONS,
                              EAXLISTENER_MAXREFLECTIONS, 1.f, 0);
        PHelper().CreateFloat(items, "Reflections\\Reflections Delay", &S.ReflectionsDelay,
                              EAXLISTENER_MINREFLECTIONSDELAY, EAXLISTENER_MAXREFLECTIONSDELAY, 0.01f, 3);
        PHelper().CreateFloat(items, "Reverb\\Reverb", &S.Reverb, EAXLISTENER_MINREVERB, EAXLISTENER_MAXREVERB, 1.f, 0);
        PHelper().CreateFloat(items, "Reverb\\Reverb Delay", &S.ReverbDelay, EAXLISTENER_MINREVERBDELAY,
                              EAXLISTENER_MAXREVERBDELAY, 0.01f, 3);
        PHelper().CreateFloat(items, "Decay\\Decay Time", &S.DecayTime, EAXLISTENER_MINDECAYTIME,
                              EAXLISTENER_MAXDECAYTIME, 0.01f, 3);
        PHelper().CreateFloat(items, "Decay\\Decay HF Ratio", &S.DecayHFRatio, EAXLISTENER_MINDECAYHFRATIO,
                              EAXLISTENER_MAXDECAYHFRATIO, 0.01f, 3);
    }

    Ext.m_ItemProps->ClearProperties();
    Ext.m_ItemProps->AssignItems(items);
    Ext.m_ItemProps->SetModifiedEvent(TOnModifiedEvent(this, &CSHSoundEnvTools::Modified));
}
//---------------------------------------------------------------------------
