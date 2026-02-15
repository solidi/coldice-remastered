// vgui_SpectatorPanel.h: interface for the SpectatorPanel class.
//
//////////////////////////////////////////////////////////////////////

#ifndef SPECTATORPANEL_H
#define SPECTATORPANEL_H

#include <VGUI_Panel.h>
#include <VGUI_Label.h>
#include <VGUI_Button.h>

using namespace vgui;

#define SPECTATOR_PANEL_CMD_NONE				0

#define SPECTATOR_PANEL_CMD_OPTIONS				1
#define	SPECTATOR_PANEL_CMD_PREVPLAYER			2
#define SPECTATOR_PANEL_CMD_NEXTPLAYER			3
#define	SPECTATOR_PANEL_CMD_HIDEMENU			4
#define	SPECTATOR_PANEL_CMD_TOGGLE_INSET		5
#define SPECTATOR_PANEL_CMD_CAMERA				6
#define SPECTATOR_PANEL_CMD_PLAYERS				7

// spectator panel sizes
#define PANEL_HEIGHT 64

#define BANNER_WIDTH	256
#define BANNER_HEIGHT	64

#define OPTIONS_BUTTON_X 96
#define CAMOPTIONS_BUTTON_X 200


#define SEPERATOR_WIDTH 15
#define SEPERATOR_HEIGHT 15


#define TEAM_NUMBER 2

class SpectatorPanel : public Panel //, public vgui::CDefaultInputSignal
{

public:
	SpectatorPanel(int x,int y,int wide,int tall);
	virtual ~SpectatorPanel();

	void			ActionSignal(int cmd);

	// InputSignal overrides.
public:
	void Initialize();
	void Update();
	


public:

	void EnableInsetView(bool isEnabled);
	void ShowMenu(bool isVisible);
	void ShowOptions(bool isVisible);

	DropDownButton		  *	m_OptionButton;
	Panel			* m_PinLine;
	CImageButton	  *	m_PrevPlayerButton;
	CImageButton	  *	m_NextPlayerButton;
	DropDownButton     *	m_CamButton;	

	CTransparentPanel *			m_TopBorder;

	ColorButton		*m_InsetViewButton;
	
	DropDownButton	*m_BottomMainButton;
	CImageLabel		*m_TimerImage;
	Label			*m_BottomMainLabel;
	Label			*m_CurrentTime;
	Label			*m_ExtraInfo;
	Panel			*m_Separator;
	Label			*m_TopLeftTitle;
	Label			*m_TopLeftSummary;

	Label			*m_TeamScores[TEAM_NUMBER];
	
	CImageLabel		*m_TopBanner;

	bool			m_menuVisible;
	bool			m_insetVisible;

	// Team selection options
	CTransparentPanel *		m_OptionsPanel;
	ColorButton *			m_AutoAssignButton;
	ColorButton *			m_JoinBlueButton;
	ColorButton *			m_JoinRedButton;
	ColorButton *			m_SpectateButton;
	ColorButton *			m_SurpriseMeButton;
	bool					m_optionsVisible;
	float					m_flSurpriseMeCooldown;
};



class CSpectatorHandler_Command : public ActionSignal
{

private:
	SpectatorPanel * m_pFather;
	int				 m_cmd;

public:
	CSpectatorHandler_Command( SpectatorPanel * panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel * panel )
	{
		m_pFather->ActionSignal(m_cmd);
	}
};

class CSpectatorHandler_SurpriseMe : public ActionSignal
{
private:
	SpectatorPanel * m_pParent;

public:
	CSpectatorHandler_SurpriseMe( SpectatorPanel * panel )
	{
		m_pParent = panel;
	}

	virtual void actionPerformed( Panel * panel );
};


#endif // !defined SPECTATORPANEL_H
