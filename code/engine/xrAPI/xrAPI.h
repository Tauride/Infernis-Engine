#ifndef xrApi_included
#define xrApi_included
#pragma once

#ifdef XRAPI_EXPORTS
#define XRAPI_API __declspec(dllexport)
#else
#define XRAPI_API __declspec(dllimport)
#endif

// One set of declarations for the game and the editors (xrAPI.dll exports these; the old _EDITOR variant
// declared an engine-exported CRender* Render for GSC's editors).
class IRender_interface;
extern XRAPI_API IRender_interface* Render;

class IRenderFactory;
extern XRAPI_API IRenderFactory* RenderFactory;

class CDUInterface;
extern XRAPI_API CDUInterface* DU;

struct xr_token;
extern XRAPI_API xr_token* vid_mode_token;

class IUIRender;
extern XRAPI_API IUIRender* UIRender;

class CGameMtlLibrary;
extern XRAPI_API CGameMtlLibrary* PGMLib;

#ifdef DEBUG
class IDebugRender;
extern XRAPI_API IDebugRender* DRender;
#endif // DEBUG

/*
// This class is exported from the xrAPI.dll
class XRAPI_API CxrAPI {
public:
        CxrAPI(void);
        // TODO: add your methods here.
};

extern XRAPI_API int nxrAPI;

XRAPI_API int fnxrAPI(void);
*/

#endif //	xrApi_included