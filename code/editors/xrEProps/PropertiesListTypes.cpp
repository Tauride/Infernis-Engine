#include "stdafx.h"
#include <SDL3/SDL_keyboard.h>


#include "../Public/PropertiesListTypes.h"
//------------------------------------------------------------------------------
            
xr_string	ShortcutValue::GetDrawText		(TOnDrawTextEvent )
{
	xr_string 	txt;// = ToStr(MxShortCutToText(value->hotkey)).c_str();
	if (value->key == 0)
	{
		txt.append("none");
		return txt;
	}
	if (value->ext.test(xr_shortcut::flCtrl))
	{
		txt.append("Ctrl+");
	}
	if(value->ext.test(xr_shortcut::flShift))
	{
		txt.append("Shift+");
	}
	if (value->ext.test(xr_shortcut::flAlt))
	{
		txt.append("Alt+");
	}

	txt += SDL_GetScancodeName((SDL_Scancode)value->key);

	return 			txt;
}

xr_string GameTypeValue::GetDrawText(TOnDrawTextEvent)
{
	string512 str;
    // Infernis is single-player: only the Single game type exists (see gametype_chooser.h).
    xr_sprintf(str, sizeof(str), "%s", GetValue().MatchType(eGameIDSingle) ? "Single" : "");
	return xr_string(str);
}
