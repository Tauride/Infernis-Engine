#ifndef XRSE_FACTORY_IMPORT_EXPORTH
#define XRSE_FACTORY_IMPORT_EXPORTH

#ifdef XRSE_FACTORY_EXPORTS
#define FACTORY_API __declspec(dllexport)
#else
#define FACTORY_API __declspec(dllimport)
#endif

extern "C" {
FACTORY_API ISE_Abstract* __stdcall create_entity(LPCSTR section);
FACTORY_API void __stdcall destroy_entity(ISE_Abstract*&);
#ifdef XRSE_FACTORY_EDITOR
FACTORY_API void SEFactoryEntry();   // the editors' build: started and stopped by the editor (xrSE_Factory.cpp)
FACTORY_API void SEFactoryDestroy();
#endif
};

#endif