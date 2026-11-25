#pragma once
class UIKeyForm :public IEditorWnd
{
public:
	UIKeyForm();
	virtual ~UIKeyForm();
	virtual void Draw();
	IC bool AutoChange() const { return m_AutoChange; }
	IC float Position() const { return m_Position; };

private:
	void SetMark(int id, int action);
	void DrawMark(int id);
#ifdef EDITOR_MOTION_NOTIFY // IX-Ray motion notify tracks (not in Infernis's motion format, GSC's)
	void DrawNotify();
	void DrawNotify(const NotifyTracksType::value_type& elem);
	void DrawNotify(const NotifyTrack& elem);
#endif
	xr_vector<float> m_TempForPlotHistogram;
	float m_Position;
	bool m_AutoChange;
	float m_TimeFactor;
	CSMotion* m_currentEditMotion;
#ifdef EDITOR_MOTION_NOTIFY
	anim_notify* m_currentNotify = nullptr;
#endif
};

