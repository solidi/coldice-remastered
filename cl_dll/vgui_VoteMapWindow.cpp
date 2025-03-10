
#include "VGUI_Font.h"
#include <VGUI_TextImage.h>

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_ServerBrowser.h"

// Class Menu Dimensions
#define CLASSMENU_TITLE_X				XRES(40)
#define CLASSMENU_TITLE_Y				YRES(32)
#define CLASSMENU_TOPLEFT_BUTTON_X		XRES(40)
#define CLASSMENU_TOPLEFT_BUTTON_Y		YRES(80)
#define CLASSMENU_BUTTON_SIZE_X			XRES(124)
#define CLASSMENU_BUTTON_SIZE_Y			YRES(24)
#define CLASSMENU_BUTTON_SPACER_Y		YRES(8)

// Creation
CVoteMapPanel::CVoteMapPanel(int iTrans, int iRemoveMe, int x,int y,int wide,int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
{
	// Get the scheme used for the Titles
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );

	// color schemes
	int r, g, b, a;

	// Create the title
	pTitleLabel = new Label( "", CLASSMENU_TITLE_X, CLASSMENU_TITLE_Y );
	pTitleLabel->setParent( this );
	pTitleLabel->setFont( pSchemes->getFont(hTitleScheme) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setBgColor( r, g, b, a );
	pTitleLabel->setContentAlignment( vgui::Label::a_west );
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMap"));

	// Create the map buttons
	for (int i = 0; i < BUILT_IN_MAP_COUNT; i++)
	{
		// Space for random button
		int xI = i+1;
		int degree = (i+1) / 12;
		if (i == BUILT_IN_MAP_COUNT - 1)
		{
			xI = 0;
			degree = 0;
		}
		char sz[256];
		int iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( (CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y) * xI );
		int spacer = 0;
		spacer = (CLASSMENU_BUTTON_SIZE_X + 10) * degree;
		iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( (CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y) * (xI - (12 * degree)));
		
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// Map button
		sprintf(sz, " %s", sBuiltInMaps[i]);
		m_pButtons[i] = new ColorButton( sz, CLASSMENU_TOPLEFT_BUTTON_X + spacer, iYPos, CLASSMENU_BUTTON_SIZE_X, CLASSMENU_BUTTON_SIZE_Y, false, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent(this);
	}

	m_iCurrentInfo = 0;
}

void CVoteMapPanel::Update()
{
	// Time
	float minutes = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	float seconds = fmax( 0, ( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));

	int votes[BUILT_IN_MAP_COUNT];
	int myVote = -1;

	// Count votes
	int highest = -1, hi = -1;
	for (int j = 0; j < BUILT_IN_MAP_COUNT; j++)
		m_pButtons[j]->setArmed(false);

	// Count votes
	for ( int i = 0; i < BUILT_IN_MAP_COUNT; i++ )
	{
		votes[i] = 0;
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if ( g_PlayerInfoList[j].thisplayer )
				myVote = g_PlayerExtraInfo[j].vote;

			if (g_PlayerExtraInfo[j].vote == (i + 1))
				votes[i] += 1;
		}
		
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if (highest < votes[i])
			{
				highest = votes[i];
				hi = i;
			}
		}
	}

	for ( int i = 0; i < BUILT_IN_MAP_COUNT; i++ )
	{
		if (m_pButtons[i])
		{
			char sz[64];
			sprintf(sz, " %-2d %s", votes[i], sBuiltInMaps[i]);
			m_pButtons[i]->setText(sz);

			if ((myVote - 1) == i)
				m_pButtons[i]->setArmed(true);

			int r, g, b, a = 0;
			UnpackRGB(r, g, b, HudColor());
			m_pButtons[i]->setUnArmedColor(r, g, b, 0);
			pTitleLabel->setFgColor( r, g, b, 0 );
			if (votes[i] > 0)
			{
				m_pButtons[i]->setArmed(true);
				if (votes[i] == highest)
				{
					m_pButtons[i]->setBorder(new LineBorder(Color(255, 255, 255, a)));
					m_pButtons[i]->setBgColor(r, g, b, 0);
					m_pButtons[i]->setArmedColor(255, 255, 255, 0);
				}
				else
				{
					m_pButtons[i]->setBorder(new LineBorder(Color(r, g, b, a)));
					m_pButtons[i]->setBgColor(r, g, b, 255);
					m_pButtons[i]->setArmedColor(r, g, b, 0);
				}
			}
			else
			{
				m_pButtons[i]->setBorder(new LineBorder( Color(r, g, b, a)));
			}
		}
	}

	pTitleLabel->setText("%s | Your Vote: %s | Time Left: %.1f\n", gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMap"), myVote > 0 ? sBuiltInMaps[myVote-1] : "None", seconds);
}

//======================================
// Key inputs for the Class Menu
bool CVoteMapPanel::SlotInput( int iSlot )
{
	if ( (iSlot < 0) || (iSlot > 9) )
		return false;

	if ( !m_pButtons[ iSlot ] )
		return false;

	//if ( !(m_pButtons[ iSlot ]->IsNotValid()) )
	{
		for (int i = 0; i < BUILT_IN_MAP_COUNT; i++)
		{
			m_pButtons[i]->setArmed( false );
		}

		m_pButtons[ iSlot ]->setArmed( true );
		m_iCurrentInfo = iSlot;
		m_pButtons[ iSlot ]->fireActionSignal();
		return true;
	}

	return false;
}

//======================================
// Update the Class menu before opening it
void CVoteMapPanel::Open( void )
{
	SetActiveInfo(0);
	Update();
	CMenuPanel::Open();
}

//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void CVoteMapPanel::Initialize( void )
{
	setVisible( false );
}

//======================================
// Mouse is over a class button, bring up the class info
void CVoteMapPanel::SetActiveInfo( int iInput )
{
	if ( iInput > (BUILT_IN_MAP_COUNT - 1) || iInput < 0 )
		iInput = 0;

	m_iCurrentInfo = iInput;
}
