////////////////////////////////////////////////////////////////////////////
//	Module 		: xrSE_Factory.cpp
//	Created 	: 18.06.2004
//  Modified 	: 18.06.2004
//	Author		: Dmitriy Iassenev
//	Description : Precompiled header creatore
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "xrSE_Factory.h"
#include "ai_space.h"
#include "script_engine.h"
#include "object_factory.h"
#include "xrEProps.h"
#include "xrSE_Factory_import_export.h"
#include "script_properties_list_helper.h"

#include "character_info.h"
#include "specific_character.h"
//#include "character_community.h"
//#include "monster_community.h"
//#include "character_rank.h"
//#include "character_reputation.h"

extern CSE_Abstract* F_entity_Create(LPCSTR section);

extern CScriptPropertiesListHelper* g_property_list_helper;
#ifndef XRSE_FACTORY_EDITOR
extern HMODULE prop_helper_module;
#endif

extern "C" {
FACTORY_API ISE_Abstract* __stdcall create_entity(LPCSTR section) {
    return (F_entity_Create(section));
}

FACTORY_API void __stdcall destroy_entity(ISE_Abstract*& abstract) {
    CSE_Abstract* object = smart_cast<CSE_Abstract*>(abstract);
    F_entity_Destroy(object);
    abstract = 0;
}
};

void setup_luabind_allocator();

// Infernis: start-up and shut-down split out of DllMain. GSC's factory initialises its own xrCore (fsfactory.ltx) when it
// is loaded; the editors' build (XRSE_FACTORY_EDITOR) runs inside an editor that already did, and is started and
// stopped by the editor (SEFactoryEntry, SEFactoryDestroy, as IX-Ray's factory is).
static bool s_own_settings = false;

static void factory_startup(bool own_core) {
    if (own_core) {
        Debug._initialize();
        Core._initialize("xrSE_Factory", NULL, TRUE, "fsfactory.ltx");
    }
    if (own_core || !pSettings) { // GSC's factory always loads its own; the editors' reuses the editor's
        string_path SYSTEM_LTX;
        FS.update_path(SYSTEM_LTX, "$game_config$", "system.ltx");
        pSettings = xr_new<CInifile>(SYSTEM_LTX);
        s_own_settings = true;
    }

    setup_luabind_allocator();

    CCharacterInfo::InitInternal();
    CSpecificCharacter::InitInternal();
}

static void factory_shutdown(bool own_core) {
    CCharacterInfo::DeleteSharedData();
    CCharacterInfo::DeleteIdToIndexData();
    CSpecificCharacter::DeleteSharedData();
    CSpecificCharacter::DeleteIdToIndexData();

    xr_delete(g_object_factory);
    if (s_own_settings) {
        CInifile** s = (CInifile**)(&pSettings);
        xr_delete(*s);
        s_own_settings = false;
    }
    xr_delete(g_property_list_helper);
    xr_delete(g_ai_space);
    xr_delete(g_object_factory);
#ifndef XRSE_FACTORY_EDITOR
    if (prop_helper_module)
        FreeLibrary(prop_helper_module);
#endif
    if (own_core)
        Core._destroy();
}

#ifdef XRSE_FACTORY_EDITOR
FACTORY_API void SEFactoryEntry() { factory_startup(false); }
FACTORY_API void SEFactoryDestroy() { factory_shutdown(false); }
#else
BOOL APIENTRY DllMain(HANDLE module_handle, DWORD call_reason, LPVOID reserved) {
    switch (call_reason) {
    case DLL_PROCESS_ATTACH:
        factory_startup(true);
        break;
    case DLL_PROCESS_DETACH:
        factory_shutdown(true);
        break;
    }
    return (TRUE);
}
#endif

void _destroy_item_data_vector_cont(T_VECTOR* vec) {
    T_VECTOR::iterator it = vec->begin();
    T_VECTOR::iterator it_e = vec->end();

    xr_vector<CUIXml*> _tmp;
    for (; it != it_e; ++it) {
        xr_vector<CUIXml*>::iterator it_f = std::find(_tmp.begin(), _tmp.end(), (*it)._xml);
        if (it_f == _tmp.end())
            _tmp.push_back((*it)._xml);
    }
    delete_data(_tmp);
}
