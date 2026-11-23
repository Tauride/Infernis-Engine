// The game's half of the autotest ui_* steps: finds the visible UI windows, dumps them,
// and turns key and action names into key codes. The engine half (input, the virtual
// cursor, the steps themselves) is in xrEngine/xr_autotest.cpp.

#include "stdafx.h"

#include "../xrEngine/xr_autotest.h"
#include "MainMenu.h"
#include "UIGameCustom.h"
#include "UIDialogHolder.h"
#include "UICursor.h"
#include "Level.h"
#include "xr_level_controller.h"
#include "ui/UIWindow.h"
#include "ui/UIStatic.h"
#include "ui/UILines.h"
#include "ui/UICustomEdit.h"
#include "ui/UICheckButton.h"
#include "ui/UIComboBox.h"
#include "ui/UIListBoxItem.h"
#include "ui/UIScrollView.h"
#include "ui/UIOptionsItem.h"
#include "ui/UICellCustomItems.h"
#include "inventory_item.h"
#include "PhysicsShellHolder.h"
#include "string_table.h"
#include "ui_base.h"
#include <typeinfo>

extern _keyboard	keyboards[];

namespace
{
	LPCSTR	class_name		(CUIWindow* w)
	{
		LPCSTR	n	= typeid(*w).name();
		if (0==strncmp(n,"class ",6))	n += 6;
		if (0==strncmp(n,"struct ",7))	n += 7;
		return	n;
	}

	// An unnamed window has a NULL shared_str
	LPCSTR	window_name		(CUIWindow* w)
	{
		LPCSTR	n	= w->WindowName().c_str();
		return	n ? n : "";
	}

	// Statics and buttons, edit boxes, text windows, list entries, a dropdown's current value
	LPCSTR	window_text		(CUIWindow* w)
	{
		LPCSTR	t	= NULL;
		if (CUICustomEdit* E = smart_cast<CUICustomEdit*>(w))			t = E->GetText();
		else if (CUIStatic* S = smart_cast<CUIStatic*>(w))				t = S->HasTextItemControl() ? S->TextItemControl()->GetText() : NULL;
		else if (CUITextWnd* T = smart_cast<CUITextWnd*>(w))			t = T->GetText();
		else if (CUIListBoxItem* L = smart_cast<CUIListBoxItem*>(w))	t = L->GetText();
		else if (CUIComboBox* C = smart_cast<CUIComboBox*>(w))			t = C->GetText();
		return	t ? t : "";
	}

	// The console command an options control (check box, track bar, list, ...) is bound to
	LPCSTR	window_option	(CUIWindow* w)
	{
		CUIOptionsItem*	O	= smart_cast<CUIOptionsItem*>(w);
		LPCSTR			e	= O ? O->GetEntry() : NULL;	// NULL until the control is bound
		return			e ? e : "";
	}

	// The string table id the text was translated from, if any: the same in every language
	LPCSTR	window_text_id	(CUIWindow* w)
	{
		LPCSTR	t	= window_text(w);
		if (!*t)	return "";
		shared_str	id	= CStringTable::FindId(shared_str(t));
		return	id.size() ? id.c_str() : "";	// the table keeps the id alive
	}

	// The section of an inventory, trade or loot cell's item (wpn_ak74, medkit, ...)
	LPCSTR	window_item		(CUIWindow* w)
	{
		CUIInventoryCellItem*	C	= smart_cast<CUIInventoryCellItem*>(w);
		LPCSTR					s	= C && C->object() ? C->object()->object().cNameSect().c_str() : NULL;
		return					s ? s : "";
	}

	bool	text_contains	(LPCSTR text, LPCSTR part)
	{
		string1024	a, b;
		strncpy_s	(a,text,_TRUNCATE);	_strlwr_s(a);
		strncpy_s	(b,part,_TRUNCATE);	_strlwr_s(b);
		return		NULL!=strstr(a,b);
	}

	// The dialogs that take input now, bottom to top: the main menu when it is open
	// (it then takes all input), otherwise the game's own dialogs
	void	roots			(xr_vector<CUIWindow*>& out)
	{
		CDialogHolder*	H	= NULL;
		CMainMenu*		M	= MainMenu();
		if (M && M->IsActive())						H = M;
		else if (g_pGameLevel && CurrentGameUI())	H = CurrentGameUI();
		if (!H)			return;
		const xr_vector<dlgItem>&	D	= H->DialogsToRender();
		for (u32 i=0; i<D.size(); ++i)
			if (D[i].enabled && D[i].wnd && D[i].wnd->IsShown())
				out.push_back	(D[i].wnd);
	}

	class CAutotestUI : public IAutotestUI
	{
		// Every visible window in draw order (the last one is drawn on top)
		struct SWnd
		{
			CUIWindow*	wnd;
			Frect		rect;		// the visible part in UI coordinates; empty when scrolled away
			u32			parent;		// index in SWalk::all, or u32(-1) for a dialog
			int			child;		// index among the parent's visible children (#n)
			int			depth;
		};
		typedef xr_vector<SWnd>	SWalk;

		// clip: the part of the screen the window can show in; scroll views cut off their children
		void	walk		(SWalk& W, CUIWindow* w, const Frect& clip, u32 parent, int child, int depth)
		{
			Frect		r;
			w->GetAbsoluteRect	(r);
			SWnd		S;
			S.wnd		= w;
			S.parent	= parent;
			S.child		= child;
			S.depth		= depth;
			if (!S.rect.intersection(r,clip))	S.rect.set(0,0,0,0);
			u32			me	= u32(W.size());
			W.push_back	(S);

			Frect		child_clip	= clip;
			if (smart_cast<CUIScrollView*>(w) && !child_clip.intersection(clip,r))
				child_clip.set		(0,0,0,0);
			int			n	= 0;
			CUIWindow::WINDOW_LIST&	L	= w->GetChildWndList();
			for (CUIWindow::WINDOW_LIST_it it=L.begin(); it!=L.end(); ++it)
				if ((*it)->IsShown())
					walk	(W,*it,child_clip,me,n++,depth+1);
		}

		void	collect		(SWalk& W)
		{
			xr_vector<CUIWindow*>	R;
			roots		(R);
			Frect		screen;
			screen.set	(0,0,UI_BASE_WIDTH,UI_BASE_HEIGHT);
			for (u32 i=0; i<R.size(); ++i)
				walk	(W,R[i],screen,u32(-1),int(i),0);
		}

		static bool	visible		(const SWnd& S)	{ return S.rect.width()>0 && S.rect.height()>0; }

		static bool	descends	(const SWalk& W, u32 i, const xr_vector<bool>& from)
		{
			for (u32 p=W[i].parent; p!=u32(-1); p=W[p].parent)
				if (from[p])	return true;
			return false;
		}

		// One path segment: "#n", "text:...", "option:..." or a window name.
		// loose: text: also matches a text that only contains it.
		static bool	matches		(const SWnd& S, const xr_string& seg, bool loose)
		{
			if (seg[0]=='#')						return S.child==atoi(seg.c_str()+1);
			if (0==strncmp(seg.c_str(),"text:",5))	return loose ? text_contains(window_text(S.wnd),seg.c_str()+5) : 0==stricmp(window_text(S.wnd),seg.c_str()+5);
			if (0==strncmp(seg.c_str(),"option:",7))	return 0==stricmp(window_option(S.wnd),seg.c_str()+7);
			if (0==strncmp(seg.c_str(),"item:",5))		return 0==stricmp(window_item(S.wnd),seg.c_str()+5);
			if (0==strncmp(seg.c_str(),"class:",6))		return 0==stricmp(class_name(S.wnd),seg.c_str()+6);
			if (0==strncmp(seg.c_str(),"st:",3)) {
				LPCSTR	t	= window_text(S.wnd);
				if (!*t)	return false;
				shared_str	v	= CStringTable::Lookup(shared_str(seg.c_str()+3));
				return	v.size() && 0==xr_strcmp(*v,t);
			}
			return 0==strcmp(window_name(S.wnd),seg.c_str());
		}

	public:
		// The target is a path of segments separated by '/': the first matches any visible
		// window, a #n segment the n-th visible child of the previous match, any other
		// segment a window anywhere below it. A text: segment takes the rest of the target.
		virtual bool	find_window		(LPCSTR target, Fvector2& center, LPSTR error, u32 error_size)
		{
			SWalk		W;
			collect		(W);
			if (W.empty()) {
				xr_sprintf	(error,error_size,"no UI dialog is open");
				return		false;
			}

			xr_vector<xr_string>	segs;
			for (LPCSTR p=target; *p; ) {
				LPCSTR	e	= 0==strncmp(p,"text:",5) ? p+xr_strlen(p) : strchr(p,'/');
				if (!e)	e	= p+xr_strlen(p);
				if (e>p)	segs.push_back(xr_string(p,e));
				p			= *e ? e+1 : e;
			}
			if (segs.empty()) {
				xr_sprintf	(error,error_size,"empty target");
				return		false;
			}
			if (segs[0][0]=='#') {
				xr_sprintf	(error,error_size,"'%s': a #n segment needs a window before it",target);
				return		false;
			}
			for (u32 s=0; s<segs.size(); ++s)
				if (segs[s][0]=='#' && (segs[s].size()<2 || segs[s].find_first_not_of("0123456789",1)!=xr_string::npos)) {
					xr_sprintf	(error,error_size,"'%s': '%s' is not #<number>",target,segs[s].c_str());
					return		false;
				}

			xr_vector<bool>	cur, next;
			cur.assign		(W.size(),false);
			next.assign		(W.size(),false);
			for (u32 s=0; s<segs.size(); ++s) {
				bool	any	= false;
				for (int loose=0; loose<2 && !any; ++loose) {
					for (u32 i=0; i<W.size(); ++i) {
						bool	ok	= matches(W[i],segs[s],!!loose);
						if (ok && s>0)
							ok	= segs[s][0]=='#' ? (W[i].parent!=u32(-1) && cur[W[i].parent]) : descends(W,i,cur);
						next[i]	= ok;
						any		= any || ok;
					}
					if (0!=strncmp(segs[s].c_str(),"text:",5))	break;
				}
				if (!any) {
					if (s==0)	xr_sprintf(error,error_size,"no visible window '%s'",segs[0].c_str());
					else		xr_sprintf(error,error_size,"'%s': nothing matches '%s' there",target,segs[s].c_str());
					return		false;
				}
				cur.swap	(next);
			}

			// The topmost one that can be seen
			for (int i=int(W.size())-1; i>=0; --i)
				if (cur[i] && visible(W[i])) {
					W[i].rect.getcenter	(center);
					return				true;
				}
			xr_sprintf	(error,error_size,"'%s' is outside the screen or scrolled out of view",target);
			return		false;
		}

		virtual void	dump			(line_cb cb, void* ctx)
		{
			SWalk		W;
			collect		(W);
			string2048	line;
			for (u32 i=0; i<W.size(); ++i) {
				const SWnd&	S		= W[i];
				CUIWindow*	w		= S.wnd;
				Frect		r;
				w->GetAbsoluteRect	(r);
				string64	indent	= "";
				for (int d=0; d<S.depth && d<30; ++d)	xr_strcat(indent," ");

				string256	state	= "";
				if (!w->IsEnabled())					xr_strcat(state," disabled");
				if (CUICheckButton* C = smart_cast<CUICheckButton*>(w))
					xr_strcat(state,C->GetCheck() ? " checked" : " unchecked");
				if (!visible(S))						xr_strcat(state," clipped");

				string256	name, option, item, text_id;
				string512	text;		// long texts are cut, the line must fit
				strncpy_s	(name,window_name(w),_TRUNCATE);
				strncpy_s	(option,window_option(w),_TRUNCATE);
				strncpy_s	(item,window_item(w),_TRUNCATE);
				strncpy_s	(text_id,window_text_id(w),_TRUNCATE);
				strncpy_s	(text,window_text(w),_TRUNCATE);
				xr_sprintf	(line,"%s#%d %s%s%s%s [%.0f,%.0f %.0fx%.0f]%s%s%s%s%s%s%s%s%s%s",indent,S.child,class_name(w),
							*name ? " '" : "",name,*name ? "'" : "",
							r.x1,r.y1,r.width(),r.height(),state,
							*option ? " option=" : "",option,
							*item ? " item=" : "",item,
							*text_id ? " st=" : "",text_id,
							*text ? " text=\"" : "",text,*text ? "\"" : "");
				cb			(ctx,line);
			}
			if (W.empty())	cb(ctx,"(no UI dialog is open)");
		}

		virtual int		key_code		(LPCSTR name)
		{
			for (int pass=0; pass<2; ++pass) {	// as written ("kESCAPE", "mouse1"), then with the k ("ESCAPE")
				string64	n;
				xr_sprintf	(n,"%s%s",pass ? "k" : "",name);
				for (int i=0; keyboards[i].key_name; ++i)
					if (0==stricmp(keyboards[i].key_name,n))	return keyboards[i].dik;
			}
			for (int i=0; actions[i].action_name; ++i)	// a bound action ("inventory")
				if (0==stricmp(actions[i].action_name,name)) {
					int	dik	= get_action_dik(actions[i].id);
					return dik ? dik : -1;
				}
			return	-1;
		}

		virtual bool	cursor_visible	()
		{
			return g_pGamePersistent && GetUICursor().IsVisible();
		}
	};

	CAutotestUI		g_autotest_ui;

	struct SRegister
	{
		SRegister	()	{ if (Autotest::active())	Autotest::set_ui(&g_autotest_ui); }
		~SRegister	()	{ if (Autotest::active())	Autotest::set_ui(NULL); }
	}				g_register;
}
