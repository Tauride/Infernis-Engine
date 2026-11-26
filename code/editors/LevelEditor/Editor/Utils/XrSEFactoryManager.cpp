#include "stdafx.h"
#include "xrServerEntities/xrServer_Objects.h"

// Infernis: the server entity factory is Infernis's xrSE_Factory, built for the editors (xrSE_Factory_Editor): GSC's
// create_entity/destroy_entity, plus SEFactoryEntry/SEFactoryDestroy, which start and stop it inside the editor.
extern "C"
{
	__declspec(dllimport) ISE_Abstract* __stdcall create_entity(LPCSTR section);
	__declspec(dllimport) void __stdcall destroy_entity(ISE_Abstract*& abstract);
	__declspec(dllimport) void SEFactoryEntry();
	__declspec(dllimport) void SEFactoryDestroy();
}

XrSEFactoryManager* g_SEFactoryManager = nullptr;

XrSEFactoryManager::XrSEFactoryManager()
{
	SEFactoryEntry();
}

XrSEFactoryManager::~XrSEFactoryManager()
{
	SEFactoryDestroy();
}

CSE_Abstract* XrSEFactoryManager::create_entity(const char* section)
{
	return smart_cast<CSE_Abstract*>(::create_entity(section));
}

void XrSEFactoryManager::destroy_entity(CSE_Abstract*& abstract)
{
	ISE_Abstract* Entity = abstract;
	::destroy_entity(Entity);
	abstract = nullptr;
}

void XrSEFactoryManager::reload()
{
	// Infernis: GSC's factory has no reload; the entity sections are read when the factory starts.
}
